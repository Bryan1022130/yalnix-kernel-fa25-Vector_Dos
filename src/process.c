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

/* ===============================================================================================================
 * pcb_alloc()
 * Returns a pointer to the first FREE PCB in the table.
 * Sets the state to READY and assigns its PID.
 * ===============================================================================================================
 */
PCB *proc_alloc(void) {
	//This is wrong
    static int next_pid = 1;  // simple incremental pid generator

    if (process_free_head == NULL) return NULL;
    PCB *p = process_free_head;
    process_free_head = p->next;
    memset(p, 0, sizeof(PCB));
    p->currState = READY;
    p->pid = next_pid++;  // assign pid here

    return p;
}

/* ===============================================================================================================
 * pcb_free(pid)
 * Frees all resources associated with a process:
 *  - Unmaps Region 1 frames
 *  - Frees its kernel stack frames
 *  - Resets PCB state to FREE
 * ===============================================================================================================
 */

void proc_free(PCB *p) {
    if (!p) return;
    p->currState = FREE;
    p->next = process_free_head;
    process_free_head = p;
}

// ----------------- Context Switching -----------------------------
PCB *get_next_ready_process(void) {
    if (isEmpty(readyQueue)) {
        return idle_process;
    }
    return Dequeue(readyQueue);
}

KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p){
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
    if (curr->currState == RUNNING){
	    //Take the next process of the Queue
	    Dequeue(readyQueue);
		
	    //Setup current process for readyQueue and enqueue 
	    curr->currState = READY;
	    Enqueue(readyQueue, curr);
    }





    if (next->curr_kc.lc.uc_mcontext.gregs[REG_ESP] == 0) {
        TracePrintf(1, "KCSwitch: first run of PID %d - copying kernel stack\n", next->pid);
        
	memcpy(&next->curr_kc, kc_in, sizeof(KernelContext));

        // Copy kernel stack from current to next
        int temp_vpn = -1;
        for (int i = (KERNEL_STACK_BASE >> PAGESHIFT) - 1; i > _orig_kernel_brk_page; i--) {
            if (kernel_page_table[i].valid == FALSE) {
                temp_vpn = i;
                break;
            }
        }

        if (temp_vpn < 0) {
            TracePrintf(0, "KCSwitch: no free VPN for stack copy!\n");
            return &next->curr_kc;
        }
        
        void *temp_va = (void *)(temp_vpn << PAGESHIFT);
        
        for (int i = 0; i < KSTACKS; i++) {
            void *src_va = (void *)(KERNEL_STACK_BASE + (i * PAGESIZE));
            
            // Map next's frame temporarily
            kernel_page_table[temp_vpn].pfn = next->kernel_stack_frames[i];
            kernel_page_table[temp_vpn].valid = 1;
            kernel_page_table[temp_vpn].prot = PROT_READ | PROT_WRITE;
            WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
            
            // Copy from curr (at KERNEL_STACK_BASE) to next
            memcpy(temp_va, src_va, PAGESIZE);
	    // Unmap
            kernel_page_table[temp_vpn].valid = 0;
            WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
        }  
            
             if (next->AddressSpace != NULL) {
        WriteRegister(REG_PTBR1, (unsigned int)next->AddressSpace);
        WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    }
    
    // Remap kernel stack
    /*

    int ks_base_vpn = (KERNEL_STACK_BASE >> PAGESHIFT);
    for (int i = 0; i < KSTACKS; i++) {
        kernel_page_table[ks_base_vpn + i].pfn = next->kernel_stack_frames[i];
        kernel_page_table[ks_base_vpn + i].prot = PROT_READ | PROT_WRITE;
        kernel_page_table[ks_base_vpn + i].valid = 1;
    }
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_KSTACK);
    */
    
    next->currState = RUNNING;
    current_process = next;
    
    TracePrintf(1, "KCSwitch: first-run complete, returning current context\n");

    return kc_in;
  }

  
    // Normal switch code for already-running processes
    if (next->AddressSpace != NULL) {
        WriteRegister(REG_PTBR1, (unsigned int)next->AddressSpace);
        WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    }

    int ks_base_vpn = (KERNEL_STACK_BASE >> PAGESHIFT);
    for (int i = 0; i < KSTACKS; i++) {
        kernel_page_table[ks_base_vpn + i].pfn = next->kernel_stack_frames[i];
        kernel_page_table[ks_base_vpn + i].prot = PROT_READ | PROT_WRITE;
        kernel_page_table[ks_base_vpn + i].valid = 1;
    }
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_KSTACK);

    next->currState = RUNNING;
    current_process = next;

    TracePrintf(1, "KCSwitch: switched from PID %d to PID %d\n", curr->pid, next->pid);
    return &next->curr_kc;
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
	memcpy(&new_pcb->curr_kc, kc_in, sizeof(KernelContext));
	
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
		TracePrintf(0, "There was an error! I could not find a free kernel vpn :(\n");
		return NULL;
	}

	//This is the byte address of our current pte that is in kernel_page_tavble
	void * vpn_table_addr = (void *)(holdvpn << PAGESHIFT);

	//.Copy over the content of the current kernel stack into frames that have been allocated
	for(int t = 0; t < KSTACKS; t++){

		TracePrintf(0, "This is the value of KERNEL_STACK_BASE ==> %lx\n", KERNEL_STACK_BASE);

		//This is the vaddr of the currently running kernel stack
		void *pagefind = (void *)(KERNEL_STACK_BASE + (t * PAGESIZE));
		TracePrintf(0, "This is the value of the pagefind ==> %lx", pagefind);
		
		//Get the current pfn from our process
		int kernel_pfn_find = new_pcb->kernel_stack_frames[t];
		
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

