//Header files from yalnix_framework && libc library
#include <sys/types.h> //For u_long
#include <load_info.h> //The struct for load_info
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ... 
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL 
#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC

//Our Header Files
#include "Queue.h"
#include "trap.h"
#include "memory.h" 
#include "process.h" 
#include "idle.h"
#include "init.h"
#include "terminal.h"

//Macros for if VM is enabled
#define TRUE 1
#define FALSE 0

//Kernel Stack Macros {Same as PCB struct}
#define KERNEL_STACK_PAGES (KERNEL_STACK_MAXSIZE / PAGESIZE) 

/* ================================>>
 * Global Variables
 * <<================================
 */

//Virtual Memory
short int vm_enabled = FALSE;

//Physical Memory
unsigned char *track_global;

//Process Block Control
PCB *current_process = NULL; 
PCB *idle_process = NULL;
PCB *process_free_head = NULL;

// global process queues
Queue *readyQueue;    
Queue *sleepQueue;

//Kernel Control
void *current_kernel_brk;
UserContext *KernelUC;

//Frames available in physical memory {pmem_size / PAGESIZE}
unsigned long int frame_count;

//how many hardware clock ticks have occurred since boot
unsigned long current_tick = 0;

//Virtual Memory look up logic
unsigned long int vp0 = VMEM_0_BASE >> PAGESHIFT;
unsigned long int vp1 = VMEM_1_BASE >> PAGESHIFT;

//Kernel Page Table {Malloc Called}
pte_t *kernel_page_table;

//Functions for terminal {TTY_TRANSMIT && TTY_RECEIVE}
Terminal t+array[NUM_TERMINALS];

//Global array for the Interrupt Vector Table
HandleTrapCall Interrupt_Vector_Table[TRAP_VECTOR_SIZE];

/* ==================================================================================================================
 * Kernel Stack free function 
 * ==================================================================================================================
 */
void free_sframes(PCB *free_proc, unsigned char *track, int track_size){
        TracePrintf(0, "Freeing the Kernel Stack for the process, this many --> %d\n", KERNEL_STACK_PAGES);
        for(int i = 0; i < KERNEL_STACK_PAGES; i++){
		int kernels_pfn = free_proc->kernel_stack_frames[i];
		TracePrintf(0, "We are freeing this physical frame -> %d");
		frame_free(track, kernels_pfn);
        }
}

/* ==================================================================================================================
 * Kernel Stack Allocation function 
 * ==================================================================================================================
 */
int create_sframes(PCB *free_proc, unsigned char *track, int track_size){
        TracePrintf(0, "Creating the Kernel Stack for the process, this many --> %d\n", KERNEL_STACK_PAGES);
        for(int i = 0; i < KERNEL_STACK_PAGES; i++){
                //Allocate a physical frame for kernel stack
		int pfn = find_frame(track, track_size);
                if(pfn == ERROR){
                        TracePrintf(0, "There is no more frames to give!\n");
                        return ERROR;
                }
                free_proc->kernel_stack_frames[i] = pfn;
        }
	return 0;
}

/* ================================================================================================================================
 * Initializing Virtual Memory
 * char *cmd_args: Vector of strings, holding a pointer to each argc in boot command line {Terminated by NULL pointer}
 * unsigned int pmem_size: Size of the physical memory of the machine {Given in bytes}
 * UserContext *uctxt: pointer to an initial UserContext structure
 * ================================================================================================================================
 */ 

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt){
	TracePrintf(0,"\n\n");
	TracePrintf(0, "--------------------------------> We are the start of the KernelStart function <---------------------------------\n");

	/* <<<---------------------------------------------------------
	 * Boot up the free frame data structure && define global vars
	 * ---------------------------------------------------------->>>
	 */
	
	//Calculate number of physical frames  
	frame_count = (pmem_size / PAGESIZE);

	//Free frames creation
	unsigned char *track = (unsigned char*)malloc(frame_count);
	track_global = track;
	init_frames(track, frame_count);

	//Setup Terminal
	TerminalSetup();

	//initialize process queues
	readyQueue = initializeQueue();
	sleepQueue = initializeQueue();

	//Store current UserContext globally
	KernelUC = uctxt;

	/* <<<---------------------------------------
	 * Set up the initial Region 0 {KERNEL SPACE}
	 * Build Region 0 mappings for kernel text, data, heap, and stack.  
	 * --> Stack	-
	 * --> Heap	-
	 * --> Data	-
	 * --> Text	-
	 * --------------------------------------->>>
	 */

	//Malloc region 0 & 1 page table
	kernel_page_table = malloc(MAX_PT_LEN * sizeof(pte_t));
	pte_t *user_page_table = malloc(MAX_PT_LEN * sizeof(pte_t));
	SetupRegion0(kernel_page_table);
	SetupRegion1(user_page_table);

	// Initialize Virtual Memory
	TracePrintf(1, "------------------------------------- WE ARE TURNING ON VIRTUAL MEMORY NOW :) -------------------------------------\n\n");
	WriteRegister(REG_VM_ENABLE, TRUE);
	vm_enabled = TRUE;
	TracePrintf(1, "###################################################################################################################\n\n");

	/* <<<------------------------------
	 * Set up the Interrupt Vector Table
	 * ------------------------------>>>
	 */

	TracePrintf(1,"+++++ We are setting up the IVT and im going to call the function | CALLED KERNELSTART \n");
	setup_trap_handler(Interrupt_Vector_Table);
	TracePrintf(1,"+++++ We have left the function and are now going to set up region 0 | CALLED KERNELSTART\n\n\n");

	/* <<<------------------------------
	 * Call SetKernelBrk()
	 * ------------------------------>>>
	 */

	TracePrintf(1, "------------------------------------- WE ARE NOW GOING TO CALL KERNELBRK :) -------------------------------------\n");

	// Initialize kernel heap pointer (current_kernel_brk)  
	uintptr_t orig_brk_address = (uintptr_t)_orig_kernel_brk_page * PAGESIZE;
	current_kernel_brk = (void *)orig_brk_address;

	int kbrk_return = SetKernelBrk(current_kernel_brk);
	if(kbrk_return != 0){
		TracePrintf(0, "There was an error in SetKernelBrk\n");
		return;
	}

	TracePrintf(1, "##################################### WE HAVE CALLED KERNELBREAK ##################################################\n");

	TracePrintf(1, "------------------------------------- TIME TO CREATE OUR PROCESS (IDLE) -------------------------------------\n");

	//Create the idle proc or process 1
	int idle_ret = idle_proc_create(track, frame_count, user_page_table, uctxt);
	if(idle_ret == ERROR){
		TracePrintf(0, "Idle process failed!\n");
		return;
	}

	if(cmd_args[0] == NULL){
		TracePrintf(0, "Nothing was passed so I will just loop cause Idle :)\n");
		idle_process->currState = READY;
		Enqueue(readyQueue, idle_process);
	}else{
		TracePrintf(0, "We have a program to execute!\n");
		TracePrintf(1, "------------------------------------- TIME TO CREATE OUR PROCESS (INIT) -------------------------------------\n");

		PCB *init_pcb = create_init_proc(track, frame_count);
		if(init_pcb == NULL){
			TracePrintf(0, "There was an error when trying to call pcb_alloc for init process");
			TracePrintf(0, "Returning to idle process\n");
			return;
		}
		TracePrintf(0, "<@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Calling KCS with KCCopy SHOULD BE ONCE @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@>\n\n\n");
		if(KernelContextSwitch(KCCopy, init_pcb, NULL) < 0) return;

		if (idle_process->pid == current_process->pid){
                	TracePrintf(0, "This is the pid of the idle process because current and idle are the same --> %d\n", current_process->pid);
			TracePrintf(0, "Just run idle\n");

        	}else{
                	TracePrintf(0, "This is the pid of current process and they are not the same as idle  --> %d\n", current_process->pid);
			TracePrintf(0, "We should load program here\n");
			if(LoadProgram(cmd_args[0], cmd_args, current_process) < 0) return;
        	}
	}
		
	//Write to hardware where init is in region 1
	WriteRegister(REG_PTBR1, (unsigned int)current_process->AddressSpace);
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);	
	memcpy(uctxt, &current_process->curr_uc, sizeof(UserContext));

	TracePrintf(1, "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ KernelStart Complete +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n\n");
	return;
}
