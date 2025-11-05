//Header files from yalnix_framework
#include <ykernel.h>
#include <hardware.h>
#include <ctype.h>
#include <load_info.h>
#include <yalnix.h>
#include <ylib.h>
#include <yuser.h>
#include <string.h>
#include "process.h"
#include "Queue.h"

//extern globals defined in kernelstart.c
extern PCB *current_process;
extern PCB *idle_process;
extern Queue *readyQueue;
extern Queue *sleepQueue;
extern pte_t *kernel_page_table;
extern PCB *process_free_head;
extern UserContext *KernelUC;
extern unsigned char *track_global;
extern unsigned long int frame_count;

#define KSTACKS (KERNEL_STACK_MAXSIZE / PAGESIZE) 
#define TRUE 1
#define FALSE 0

KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p){
    TracePrintf(1, "This is the start of the KCSwitch ++++++++++++++++++++++++++++++++++++++++++++>\n");
    PCB *curr = (PCB *)curr_pcb_p;
    PCB *next = (PCB *)next_pcb_p;

    //Error Checking
    if (next == NULL) {
        TracePrintf(0, "KCSwitch: next NULL, switching to idle_process\n");
        next = idle_process;
    }

    if (curr == NULL) {
        TracePrintf(0, "KCSwitch: curr NULL, using current_process\n");
        curr = current_process;
    }

    if (curr == next) {
        TracePrintf(1, "KCSwitch: same process, skipping\n");
        return kc_in;
    }

    //copy the bytes of the kernel context into the current process’s PCB 
    memcpy(&curr->curr_kc, kc_in, sizeof(KernelContext));

   int ks_base_vpn = (KERNEL_STACK_BASE >> PAGESHIFT);
   TracePrintf(0, "This is the value of the base_vpn --> %d\n", ks_base_vpn);
    for (int i = 0; i < KSTACKS; i++) {
        kernel_page_table[ks_base_vpn + i].pfn = next->kernel_stack_frames[i];
        kernel_page_table[ks_base_vpn + i].prot = PROT_READ | PROT_WRITE;
        kernel_page_table[ks_base_vpn + i].valid = 1;
    }
    
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_KSTACK);

    WriteRegister(REG_PTBR1, (unsigned int)next->AddressSpace);
    WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

    //Update next as the current process and runnning
    next->currState = RUNNING;
    current_process = next;

    TracePrintf(1, "[KCSwitch]: switched from PID %d to PID %d\n", curr->pid, next->pid);
    TracePrintf(1, "This is the end of the KCSwitch ++++++++++++++++++++++++++++++++++++++++++++>\n\n\n");
    return &next->curr_kc;
}

KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used){
	TracePrintf(1, "This is the start of the KCCopy ++++++++++++++++++++++++++++++++++++++++++++>\n");

	if(kc_in == NULL){
		TracePrintf(0, "In KCCopy kc_in was null");
		return NULL;
	}

	//Check if the process is valid
	PCB *new_pcb = (PCB *)new_pcb_p;
	if(new_pcb == NULL){
		TracePrintf(0, "Error with one of the PCB being NULL!");
		return NULL;
	}

	//Copy over the the KernelContext into the new_pcb with a memcpy
	new_pcb->curr_kc = *kc_in;
	
	int holdvpn = (((KERNEL_STACK_BASE) >> PAGESHIFT) - 1);
	int stack_base = KERNEL_STACK_BASE >> PAGESHIFT;

	//char *src = (char *)KERNEL_STACK_BASE;
	int num_stack_pages = KERNEL_STACK_MAXSIZE / PAGESIZE;

	//pte_t *region0_pt = (pte_t *)ReadRegister(REG_PTBR0);

	TracePrintf(0,"This is the value of holdvpn --> %d\n", holdvpn);

	//Copy over the content of the current kernel stack into frames that have been allocated
	for(int t = 0; t < num_stack_pages; t++){
		//Get the current pfn from our process && current proc
		int kernel_curr_pfn = current_process->kernel_stack_frames[t];
		int kernel_new_pfn = new_pcb->kernel_stack_frames[t];
		TracePrintf(0, "This is the kernel pfn opf curr -> %d, and this is pfn of new -> %d", kernel_curr_pfn, kernel_new_pfn);
			
		//Map this value in kernel virtual memory 
		kernel_page_table[holdvpn].pfn = kernel_new_pfn;
		kernel_page_table[holdvpn].valid = TRUE;
		kernel_page_table[holdvpn].prot = PROT_READ | PROT_WRITE;
		WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

		//This is the vaddr of the currently running kernel stack
		unsigned long int pagefind = (stack_base + t) << PAGESHIFT;
		TracePrintf(0, "This is the value of the pagefind ==> %lx\n", pagefind);
		
		//We are copying the content of the kernel stack table from
		memcpy((void *)((holdvpn) << PAGESHIFT), (void *)pagefind, PAGESIZE);

		//Reset the virtual kernel page table
		kernel_page_table[holdvpn].pfn = 0;
		kernel_page_table[holdvpn].valid = FALSE;
		kernel_page_table[holdvpn].prot = 0;
		WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
	}

	Enqueue(readyQueue, new_pcb);
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

	//return the kc_in as stated in the manual
	TracePrintf(0,"This is the end of the KCCopy ++++++++++++++++++++++++++++++++++++++++++++++++++++++>\n\n\n");
	return kc_in;
}


//Process Spawning Logic 
PCB* spawn_proc(pte_t *user_page_table){
	//Make and clear new proc
	PCB *proc = (PCB *)malloc(sizeof(PCB));
	if(proc == NULL){
		TracePrintf(0, "Error with creating process! Returning NULL\n");
		return NULL;
	}

	memset(proc, 0, sizeof(PCB));

	//Set it properties
	memcpy(&proc->curr_uc, KernelUC, sizeof(UserContext));
	proc->AddressSpace = user_page_table;
	proc->pid = helper_new_pid(user_page_table);
	proc->wake_tick = 0;
	
	//Create its stack frames
	if(create_sframes(proc, track_global, frame_count) == ERROR){
		TracePrintf(0, "Error with creating stack frames for proc -> %d\n", proc->pid);
		free(proc);
		return NULL;
	}

	TracePrintf(0, "Process with PID: %d was created\n", proc->pid);
	TracePrintf(0, "There are its stack frames pfns -> %d and -> %d\n", proc->kernel_stack_frames[0], proc->kernel_stack_frames[1]);
	TracePrintf(0, "We are now going to exit the spawn_process function!\n");
}

void free_proc(PCB *proc){
	if(proc == NULL){
		TracePrintf(0, "ERROR! You are trying to free a NULL process!\n");
	}

	TracePrintf(0, "We are going to free the process with a pid of --> %d\n", proc->pid);
	memset(proc, 0, sizeof(PCB));
	free(proc);

	//Have some logic for taking off Queue or if we make a PCB proc table	
	TracePrintf(0, "Everything went well! We are leaving the free_proc() process!\n");
	return;
}
