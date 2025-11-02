//Header files from yalnix_framework && libc library
#include <sys/types.h> //For u_long
#include <ctype.h> // <----- NOT USED RIGHT NOW ----->
#include <load_info.h> //The struct for load_info
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL
#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC

//Our Header Files
#include "Queue.h" //API calls for our queue data structure
#include "trap.h" //API for trap handling and initializing the Interrupt Vector Table
#include "memory.h" //API for Frame tracking in our program
#include "process.h" //API for process block control

//Macros
#define FALSE 0
#define TRUE 1

extern pte_t *kernel_page_table;
extern PCB *current_process;
extern UserContext *KernelUC; 

/*
PCB *createInit(void){
        TracePrintf(0, "We are creating the init process {This should be process 2}\n");

        PCB *init_proc = pcb_alloc();
        if(init_proc == NULL){
                TracePrintf(0, "There was an error when getting a process for Init Process\n");
                return NULL;
        }

	// -------------------------------->> Setting up Region Table
	
         // Find a free virtual page in kernel space to map this frame
         int temp_vpn = -1;

         //Look downward for free space to not reuse pages by accident
         //Once again make this with a data structure
         for (int i = (KERNEL_STACK_BASE >> PAGESHIFT) - 1; i > _orig_kernel_brk_page; i--) {
                 if (kernel_page_table[i].valid == FALSE) {
                         temp_vpn = i;
                         break;
                 }
         }

        if (temp_vpn < 0) {
                TracePrintf(0, "idle_proc_create(): ERROR no free kernel vpn for PT mapping\n");
                return NULL;
        }

	TracePrintf(0, "We are going to get a new pid from the helper\n");
	
	//Ask the hardware for a pid and let it know a new process is being spawned
//	init_proc->pid = helper_new_pid((pte_t *)(temp_vpn << PAGESHIFT));
	init_proc->pid = 0;
        // Allocate a physical frame for the page table
        int pt_pfn = frame_alloc(init_proc->pid);

         if (pt_pfn == ERROR) {
             TracePrintf(0, "idle_proc_create(): ERROR allocating PT frame\n");
             pcb_free(init_proc->pid);
             return NULL;
         }

        //Map the pfn into the kernel_page_table so that it can be accessed by MMU
        kernel_page_table[temp_vpn].pfn = pt_pfn;
        kernel_page_table[temp_vpn].prot = PROT_READ | PROT_WRITE;
        kernel_page_table[temp_vpn].valid = TRUE;

        TracePrintf(0, "Flushing Region 1 memory space so that it knows its updated\n");
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

        // Get pointer to the page table; we are getting the virtual address with temp_vpn << PAGESHIFT
        //This serves as the blueprint to talk to physical memory
	TracePrintf(0, "IF I AM CALLED THEN I AM WHAT CAUSE THE SEGFAULT AND I KNOW WHAT TO CHANGE\n");
        pte_t *init_pt = (pte_t *)(temp_vpn << PAGESHIFT);
	TracePrintf(0, "IF I AM CALLED THEN I AM WHAT CAUSE THE SEGFAULT AND I KNOW WHAT TO CHANGE\n");

	//clear out the region one page table and make it invalid
	for(int x = 0; x < MAX_PT_LEN; x++){
		init_pt[x].prot = 0;
		init_pt[x].prot = 0;
		init_pt[x].prot = 0;
	}

	//Set up its Kernel Frames
	create_sframes(init_proc->pid, init_proc);
	
	//Figure out how to get curr_uc
	//Copy over info to new init_proc
	KCCopy(&current_process->curr_kc, init_proc, NULL);

	WriteRegister(REG_PTBR1, (unsigned int)init_proc->AddressSpace);
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);


	//Set up information for the PCB
	memcpy(&init_proc->curr_uc, KernelUC, sizeof(UserContext)); // Copy in the curr UserContext
        init_proc->AddressSpace = (void *)(temp_vpn << PAGESHIFT);
	init_proc->currState = RUNNING;
	init_proc->ppid = 0; //It has not parent process
	
	init_proc->parent = NULL;
	init_proc->first_child = NULL;
	init_proc->next_sibling = NULL;
	init_proc->wake_tick = 0;

	current_process = init_proc;
        return init_proc;
}
*/

void init(void){
	while(1){
		TracePrintf(0, "This called when there is no arguments passed to ./yalnix\n");
		Pause();
	}

}

