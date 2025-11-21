//Header files from yalnix_framework
#include <ykernel.h>
#include <hardware.h>
#include <ctype.h>
#include <load_info.h>
#include <yalnix.h>
#include <string.h>
#include "process.h"
#include "Queue.h"

//extern globals defined in kernelstart.c
extern PCB *current_process;
extern PCB *idle_process;
extern Queue *readyQueue;
extern Queue *blockedQueue;
extern pte_t *kernel_page_table;
extern PCB *process_free_head;
extern UserContext *KernelUC;
extern unsigned char *track_global;

#define KSTACKS (KERNEL_STACK_MAXSIZE / PAGESIZE) 
#define TRUE 1
#define FALSE 0

KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p) {
    TracePrintf(1, "This is the start of the KCSwitch ++++++++++++++++++++++++++++++++++++++++++++>\n");
    PCB *curr = (PCB *)curr_pcb_p;
    PCB *next = (PCB *)next_pcb_p;

    //Error Checking
    if (next == NULL) {
        TracePrintf(0, "KCSwitch: next NULL, switching to idle_process\n");
        next = idle_process;
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

KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *) {
	TracePrintf(1, "This is the start of the KCCopy ++++++++++++++++++++++++++++++++++++++++++++>\n");

	if (kc_in == NULL) {
		TracePrintf(0, "In KCCopy kc_in was null");
		return NULL;
	}

	//Check if the process is valid
	PCB *new_pcb = (PCB *)new_pcb_p;
	if (new_pcb == NULL) {
		TracePrintf(0, "Error with one of the PCB being NULL!");
		return NULL;
	}

	//Copy over the the KernelContext into the new_pcb with a memcpy
	new_pcb->curr_kc = *kc_in;
	
	int holdvpn = (((KERNEL_STACK_BASE) >> PAGESHIFT) - 1);
	int stack_base = KERNEL_STACK_BASE >> PAGESHIFT;
	int num_stack_pages = KERNEL_STACK_MAXSIZE / PAGESIZE;

	//Copy over the content of the current kernel stack into frames that have been allocated
	for (int t = 0; t < num_stack_pages; t++) {
		//Get the current pfn from our process && current proc
		int kernel_curr_pfn = current_process->kernel_stack_frames[t];
		int kernel_new_pfn = new_pcb->kernel_stack_frames[t];
			
		//Map this value in kernel virtual memory 
		kernel_page_table[holdvpn].pfn = kernel_new_pfn;
		kernel_page_table[holdvpn].valid = TRUE;
		kernel_page_table[holdvpn].prot = PROT_READ | PROT_WRITE;
		WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

		//This is the vaddr of the currently running kernel stack
		unsigned long int pagefind = (stack_base + t) << PAGESHIFT;
		
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

// ======================================================================================> Process Spawning Logic 
PCB* spawn_proc(void) {

	PCB *proc = calloc(1, sizeof(PCB));
	if (proc == NULL) {
		TracePrintf(0, "Error with creating process! Returning NULL\n");
		return NULL;
	} 

	pte_t *reg1_proc = calloc(1, MAX_PT_LEN * sizeof(pte_t));
	if(reg1_proc == NULL) {
		TracePrintf(0, "Error with creating region 1 space! Returning NULL\n");
		free(proc);
		return NULL;
	}

	//Setup new process properties
	proc->AddressSpace = reg1_proc;
	proc->pid = helper_new_pid(reg1_proc);
	proc->wake_tick = 0;

	//Create its stack frames
	if (create_sframes(proc, track_global) == ERROR) {
		TracePrintf(0, "Error with creating stack frames for proc -> %d\n", proc->pid);
		free(proc);
		free(reg1_proc);
		return NULL;
	}

	TracePrintf(0, "============================================ Debug Info =========================================================\n");
	TracePrintf(0, "Process with PID: %d was created\n", proc->pid);
	TracePrintf(0, "There are its stack frames pfns -> %d and -> %d\n", proc->kernel_stack_frames[0], proc->kernel_stack_frames[1]);
	TracePrintf(0, "We are now going to exit the spawn_process function!\n");
	TracePrintf(0, "============================================ Debug Info End =====================================================\n");

	return proc;
}

//Need to add more to this {Might need to return a int to signify success}
void free_proc(PCB *proc, int free_flip) {
	TracePrintf(0, "+++++++++++++++++++++++++++++FREE PROC++++++++++++++++++++++++++++++++++\n");
	TracePrintf(0, "We are going to free the process with a pid of --> %d\n", proc->pid);

	if(proc == NULL) {
		TracePrintf(0, "ERROR! You are trying to free a NULL process!\n");
		return;
	}

	//Unmap its region 1 and free frames
	pte_t *region1 = proc->AddressSpace;
	for (int x = 0; x < MAX_PT_LEN; x++) {
		if(!region1[x].valid) continue;
		frame_free(track_global, region1[x].pfn);
		region1[x].prot = 0;
		region1[x].pfn = 0;
		region1[x].valid = 0;
	}

	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
	
	//Free its region 1 space
	free(proc->AddressSpace);

	//free pid
	TracePrintf(0, "PID we're retiring : %d\n", proc->pid);
	helper_retire_pid(proc->pid);
	
	//Remove from all queues
	remove_data(readyQueue, (void *)proc);
	remove_data(blockedQueue, (void *)proc);

	if (free_flip) {
		//free its kernelstack frames
		free_sframes(proc, track_global);
		
		//Free the proc itself
		memset(proc, 0, sizeof(PCB));
		free(proc);
	}

	TracePrintf(0, "Everything went well! We are leaving the free_proc() process!\n");
	TracePrintf(0, "+++++++++++++++++++++++++++++FREE PROC END++++++++++++++++++++++++++++++++++\n");
	return;
}

// ======================================================================================> Process Queue Logic 
PCB *get_next_ready_process(void) {
	if (!isEmpty(readyQueue)) {
		PCB *ready_proc = Dequeue(readyQueue);
		TracePrintf(0, "The process at the start of the readyQueue is -> %d\n", ready_proc->pid);
		return ready_proc;
	}

	TracePrintf(0, "The readyQueue is empty. Returing NULL!");
	return NULL;
}
