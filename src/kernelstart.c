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
#include "idle.h"
#include "init.h"

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
static unsigned int terminal_array[NUM_TERMINALS];

//Global array for the Interrupt Vector Table
HandleTrapCall Interrupt_Vector_Table[TRAP_VECTOR_SIZE];

/* ==================================================================================================================
 * Kernel Stack Allocation function 
 * ==================================================================================================================
 */

int create_sframes(PCB *free_proc, unsigned char *track, int track_size){
        TracePrintf(0, "Creating the Kernel Stack for the Process this many --> %d\n", KERNEL_STACK_PAGES);
        for(int i = 0; i < KERNEL_STACK_PAGES; i++){
                //Allocate a physical frame for kernel stack
		int pfn = find_frame(track, track_size);
		frame_alloc(track, pfn);
		free_proc->kernel_stack_frames[i] = pfn;
                if(pfn == ERROR){
                        TracePrintf(0, "There is no more frames to give!\n");

                        //free the pcb itself
                        //pcb_free(pid);

                        return ERROR;
                }

		frame_alloc(track, pfn);
                free_proc->kernel_stack_frames[i] = pfn;
        }
	return 0;
}

/* ==========================================================================================================================================================
 * Initializing Virtual Memory
 * char *cmd_args: Vector of strings, holding a pointer to each argc in boot command line {Terminated by NULL pointer}
 * unsigned int pmem_size: Size of the physical memory of the machine {Given in bytes}
 * UserContext *uctxt: pointer to an initial UserContext structure
 * ==========================================================================================================================================================
 */ 

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt){
	TracePrintf(0, "---------> We are the start of the KernelStart function <-----------");

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

	/* <<<------------------------------
	 * Set up the Interrupt Vector Table
	 * ------------------------------>>>
	 */

	TracePrintf(1,"+++++ We are setting up the IVT and im going to call the function | CALLED KERNELSTART \n");
	setup_trap_handler(Interrupt_Vector_Table);
	TracePrintf(1,"+++++ We have left the function and are now going to set up region 0 | CALLED KERNELSTART\n\n");

	/* <<<------------------------------
	 * Call SetKernelBrk()
	 * ------------------------------>>>
	 */

	TracePrintf(1, "------------------------------------- WE ARE NOW GOING TO CALL KERNELBRK :) -------------------------------------\n");

	// Initialize kernel heap pointer (current_kernel_brk)  
    	/*
	uintptr_t orig_brk_address = (uintptr_t)_orig_kernel_brk_page * PAGESIZE;
	current_kernel_brk = (void *)orig_brk_address;

	int kbrk_return = SetKernelBrk(current_kernel_brk);
	if(kbrk_return != 0){
		TracePrintf(0, "There was an error in SetKernelBrk\n");
		return;
	}
	*/
	TracePrintf(1, "##################################### WE HAVE CALLED KERNELBREAK ##################################################\n\n");

	TracePrintf(1, "------------------------------------- TIME TO CREATE OUR PROCESS-------------------------------------\n");

	//Create the idle proc or process 1
	idle_proc_create(track, frame_count, user_page_table, uctxt);

	PCB *init_pcb = create_init_proc(user_page_table, track, frame_count);
	if(init_pcb == NULL){
		TracePrintf(0, "There was an error when trying to call pcb_alloc for init process");
		Halt();
	}


	if(cmd_args[0] == NULL){
		TracePrintf(0 ,"No argument was passed! Calling the init default function\n");
		init();
	}
	
	TracePrintf(0, "Great this the name of your program --> %s\n", cmd_args[0]);
	TracePrintf(0, "I am going to load your program \n");
	int lp_ret = LoadProgram(cmd_args[0], cmd_args, current_process);

	if(lp_ret == ERROR){
		TracePrintf(0, "ERROR WITH LOAD PROGRAM CALL\n");
		return;
	}

	// make sure hardware points to init’s page table
	WriteRegister(REG_PTBR1, (unsigned int)current_process->AddressSpace);
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

	memcpy(uctxt, &current_process->curr_uc, sizeof(UserContext));

	TracePrintf(1, "KernelStart complete.\n");
	return;
}
