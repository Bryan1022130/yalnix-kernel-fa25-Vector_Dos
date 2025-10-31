//Header files from yalnix_framework
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

// extern globals defined in kernelstart.c
extern PCB *current_process;
extern PCB *idle_process;
extern Queue *readyQueue;
extern Queue *sleepQueue;
extern pte_t kernel_page_table[MAX_PT_LEN];
extern PCB process_table[MAX_PROCS];
extern PCB *process_free_head;

#define KSTACKS (KERNEL_STACK_MAXSIZE / PAGESIZE) 
#define TRUE 1
#define FALSE 0

//-------------------------------------------------------------------------------------------PCB Logic for KernelStart

/* ===================================================================================================================
 * Process Logic Functions
 * InitPcbTable()
 * Clears all PCBs in the process table and marks them as FREE.
 * ===================================================================================================================
 */

void InitPcbTable(void){
    // Zero out the entire array
    memset(process_table, 0, sizeof(process_table));

    // Set all entries in the pcb table as free
    for (int i = MAX_PROCS - 1; i >= 0 ; i--) {
        process_table[i].currState = FREE;

        //Set up the internal linked list
        process_table[i].prev = NULL;
        process_table[i].next = process_free_head;
        process_free_head = &process_table[i];

    }

    TracePrintf(0, "Initialized PCB table: all entries marked FREE.\n");
}

/* ===============================================================================================================
 * pcb_alloc()
 * Returns a pointer to the first FREE PCB in the table.
 * Sets the state to READY and assigns its PID.
 * ===============================================================================================================
 */
PCB *pcb_alloc(void){

        //Check if there is PCBs left to use
        if(process_free_head == NULL){
                TracePrintf(0, "There was an error and no PCB were found! No PCB free!\n");
                return NULL;
        }

        TracePrintf(1, "Allocating new PCB...\n");

        PCB *free_proc = process_free_head;

        //Update the head to the next pointer
        process_free_head = process_free_head->next;

        //Disconnect from other nodes
        free_proc->next = NULL;
        free_proc->prev = NULL;

        //Clear out the data in case there is left over data
        memset(free_proc, 0, sizeof(PCB));

        //Assign the new pid and set the state as running
        free_proc->currState = READY;

        int pid_store = free_proc - process_table;
        TracePrintf(0, "This is the value of the pid from our pcb_alloc functio - > %d\n", pid_store);

        free_proc->pid = pid_store;

        TracePrintf(0, "Creating the Kernel Stack for the Process this many --> %d\n", KSTACKS);
        for(int i = 0; i < KSTACKS; i++){
                //Allocate a physical frame for kernel stack
                int pfn = frame_alloc(pid_store);

                if(pfn == ERROR){
                        TracePrintf(0, "We ran out of frames to give!\n");

                        //Unmap any previous frames that we allocated
                        for(int f = 0; f < i; f++){
                                frame_free(free_proc->kernel_stack_frames[f]);
                        }

                        //free the pcb itself
                        pcb_free(pid_store);

                        return NULL;
                }

                free_proc->kernel_stack_frames[i] = pfn;
        }

        TracePrintf(0, "Allocated PCB with PID %d \n", pid_store);
        return free_proc;
}

/* ===============================================================================================================
 * pcb_free(pid)
 * Frees all resources associated with a process:
 *  - Unmaps Region 1 frames
 *  - Frees its kernel stack frames
 *  - Resets PCB state to FREE
 * ===============================================================================================================
 */
int pcb_free(int pid){

    if(pid < 0 || pid >= MAX_PROCS){
        TracePrintf(0, "Invalid PID passed to pcb_free().\n");
        return ERROR;
    }

    //Index into buffer to get the PCB at index pid
    PCB *proc = &process_table[pid];

    if(proc->currState == FREE){
        TracePrintf(1, "Process %d already FREE.\n");
        return ERROR;
    }

    if(proc->AddressSpace == NULL){
            return ERROR;
    }

    //Get the VPN
    int vpnfind = (uintptr_t)proc->AddressSpace >> PAGESHIFT;

    //Get the PFN from the VPN in the kernel page table
    int kernel_proc_pfn = kernel_page_table[vpnfind].pfn;

    // Now access the page table
    pte_t *pt_r1 = (pte_t *)proc->AddressSpace;

    // Free all frames mapped in the process's Region 1
    int num_r1_pages = MAX_PT_LEN - 1; // This is based on the diagram but can be changed if causes problems
    for (int vpn1 = 0; vpn1 < num_r1_pages; vpn1++) {
            if (pt_r1[vpn1].valid) {
                    frame_free(pt_r1[vpn1].pfn);
                    pt_r1[vpn1].valid = FALSE;
                    pt_r1[vpn1].prot = 0;
                    pt_r1[vpn1].pfn = 0;

            }
    }

    //Set the kernel page that we used as invalid again
    kernel_page_table[vpnfind].valid = FALSE;
    kernel_page_table[vpnfind].prot = 0;
    kernel_page_table[vpnfind].pfn = 0;

    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

    // Free the physical frame that held the page table itself
    // This is a field inside of the pte_t.pfn
    frame_free(kernel_proc_pfn);

    //Free the PCBs Kernel Stack Frame
    for (int i = 0; i < KSTACKS; i++) {
            if (proc->kernel_stack_frames[i] > 0) {
                    frame_free(proc->kernel_stack_frames[i]);
                    proc->kernel_stack_frames[i] = 0;
            }
    }

    // Clear the PCB
    memset(proc, 0, sizeof(PCB));
    proc->currState = FREE;

    //Add back into the linked list of free Processes
    proc->prev = NULL;
    proc->next = process_free_head;

    //Make the freed process the head
    process_free_head = proc;

    TracePrintf(1, "Freed PCB for PID %d.\n");
    return 0;
}

// ----------------- Context Switching -----------------------------
PCB *get_next_ready_process(void) {
    if (isEmpty(readyQueue)) {
        return idle_process;   // nothing ready? run idle
    }
    return Dequeue(readyQueue);
}

KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p){
	// Save current kernel context into its PCB, restore the next PCB’s context,
	// and return pointer to the next context to run.

	//Check if the process is valid
	PCB *curr = (PCB *)curr_pcb_p;
	PCB *next = (PCB *)next_pcb_p;


	// Defensive check dont try to switch to or from NULL
	if(!curr || !next){
		TracePrintf(0, "KCSwitch: invalid PCB pointers\n");
		return kc_in;
	}
	// Defensive check skip if both PCBs are the same
	if (curr == next){
		TracePrintf(1, "KCSwitch: same process, skipping\n");
		return kc_in;
	}
	// Save current kernel context into PCB to resume later
	memcpy(&curr->curr_kc, kc_in, sizeof(KernelContext));

	//Marks old process as ready to run again
	if (curr->currState == RUNNING)
		curr->currState = READY;

	// Switches Region 1 to the next process's page table
	// Changes the virtual memory context the CPU sees
	if (next->AddressSpace != NULL){
		WriteRegister(REG_PTBR1, (unsigned int) next->AddressSpace);
		WriteRegister(REG_PTLR1, (unsigned int) MAX_PT_LEN);
		WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
	}else {
		TracePrintf(0, "KCSwitch: Warning - next process has NULL AddressSpace\n");
	}

	// Marks the new process as active and updates the global pointer that syscalls refrence 
	next->currState = RUNNING;
	current_process = next;

	TracePrintf(1, "KCSwitch: switched from PID %d to PID %d\n",
		curr->pid, next->pid);

    // Update global
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

