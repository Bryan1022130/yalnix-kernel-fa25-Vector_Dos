//Header files from yalnix_framework
#include <ykernel.h>
#include <hardware.h>
#include <ctype.h>
#include <load_info.h>
#include <yalnix.h>
#include <ylib.h>
#include <yuser.h>
#include "process.h"

#define KSTACKS (KERNEL_STACK_MAXSIZE / PAGESIZE) 

// ----------------- Context Switching -----------------------------

KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p){
	// Save current kernel context into its PCB, restore the next PCB’s context,
	// and return pointer to the next context to run.

	//Check if the process is valid
	PCB *current_pcb = (PCB *)curr_pcb_p;
	PCB *next_pcb = (PCB *)next_pcb_p;

	if(current_pcb == NULL || next_pcb == NULL){
		TracePrintf(0, "Error with one of the PCB being NULL!");
	}

	//1. Copy over the current Contents of the KernelContext into the current PCB
	//1. This will be done using a memcpy into the PCB struct )
	
	//2. Return a Pointer to the KernelContext
	
}

KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used){
	// Copy kernel context (and later, kernel stack) for a newly forked process.
	// This produces a child that resumes right after Fork().

	//Check if the process is valid
	PCB *new_pcb = (PCB *)new_pcb_p;

	if(new_pcb == NULL){
		TracePrintf(0, "Error with one of the PCB being NULL!");
	}

	//Copy over the the KernelContext into the new_pcb with a memcpy
	//Copy the Kernel Context into the new pcb
	memcpy(new_pcb.curr_kc, kc_in, sizeof(KernelContext));
	
	//find a temp vpn to map data from the current kernel stack into the frames that have been allocated for the new process's kernel stack 
	// We need to make a better way for this
	int holdvpn = -1;

	for(int i = (KERNEL_STACK_BASE >> PAGESHIFT) - 1; i > _orig_kernel_brk_page; i--){
		if(kernel_page_table[i].valid == FALSE){
			TracePrintf(0, "I found a pid at this value ==> %d", i);
			holdvpn = i;
			break;
		}
	}

	if(holdvpn < 0){
		TracePrintf("There was an error! I could not find a free kernel vpn :(\n");
		return NULL;
	}

	//This is the byte address of our current pte that is in kernel_page_tavble
	void * vpn_table_addr = (void *)(holdvpn << PAGESHIFT);

	//.Copy over the content of the current kernel stack into frames that have been allocated
	for(int t = 0; t < KSTACKS; t++){

		TracePrintf(0, "This is the value of KERNEL_STACK_BASE ==> %lx\n", KERNEL_STACK_BASE);

		//This is the vaddr of the currently running kernel stack
		void *pagefind = (void *)(KERNEL_STACK_BASE + (t * PAGESIZE));
		TracePrint(0, "This is the value of the pagefind ==> %lx", pagefind);
		
		//Get the current pfn from our process
		int kernel_pfn_find = new_pcb.kernel_stack_frames[t];
		
		//Map this value in kernel virtual memory 
		kernel_page_table[holdvpn].pfn = kernel_pfn_find;
		kernel_page_table[holdvpn].valid = TRUE;
		kernel_page_table[holdvpn].prot = PROT_READ | PROT_WRITE;
		WriteRegister(REG_TLB_FLUSH, (unsigned int)vpn_table_addr);
		
		//We are copying the content of the kernel stack table from
		//pagefind into our kernel stack memory 
		memcpy(vpn_table_addr, pagefind, PAGESIZE);
		
		//Reset the virtual kernel page table
		kernel_page_table[holdvpn].pfn = 0;
		kernel_page_table[holdvpn].valid = FALSE;
		kernel_page_table[holdvpn].prot = 0;
	}
	
	//return the kc_in as stated in the manual
	return kc_in;
}
