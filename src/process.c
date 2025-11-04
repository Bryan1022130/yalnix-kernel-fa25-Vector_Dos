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

    // Mark old process as ready to run again
    /*
    	TracePrintf(0, "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	TracePrintf(0, "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	TracePrintf(0, "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	TracePrintf(0, "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	TracePrintf(0, " We are going to Enqueue for this process -- %d\n", curr->pid);
	curr->currState = READY;
	Enqueue(readyQueue, curr);
	*/
	

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

    TracePrintf(1, "KCSwitch: switched from PID %d to PID %d\n", curr->pid, next->pid);
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
	}

	//Copy over the the KernelContext into the new_pcb with a memcpy
	memcpy(&new_pcb->curr_kc, kc_in, sizeof(KernelContext));
	
	int holdvpn = (((KERNEL_STACK_BASE) >> PAGESHIFT) - 1);
	int stack_base = KERNEL_STACK_BASE >> PAGESHIFT;

	TracePrintf(0,"This is the value of holdvpn --> %d\n", holdvpn);

	//Copy over the content of the current kernel stack into frames that have been allocated
	for(int t = 0; t < KSTACKS; t++){
		//Get the current pfn from our process && current proc
		int kernel_curr_pfn = current_process->kernel_stack_frames[t];
		int kernel_new_pfn = new_pcb->kernel_stack_frames[t];
		TracePrintf(0, "THis is the kernel pfn opf curr -> %d, and this is pfn of new -> %d", kernel_curr_pfn, kernel_new_pfn);
			
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

	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
	//return the kc_in as stated in the manual
	TracePrintf(0,"This is the end of the KCCopy ++++++++++++++++++++++++++++++++++++++++++++++++++++++>\n\n\n");
	return kc_in;
}

