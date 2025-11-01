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

//Macros for if VM is enabled
#define TRUE 1
#define FALSE 0

//Kernel Stack Macros {Same as PCB struct}
#define KERNEL_STACK_PAGES (KERNEL_STACK_MAXSIZE / PAGESIZE) 

/*
 * ================================>>
 * Global Variables
 * <<================================
 */

// These globals represent persistent kernel-wide state: 
// memory mappings, process table, and heap/stack pointers.  
// They are shared across modules and exist for the kernel’s lifetime.

int pmem_size1;
//Virtual Memory Check
short int vm_enabled = FALSE;

//Process Block
PCB *current_process = NULL; 
PCB *idle_process = NULL;
PCB *process_free_head = NULL;
//PCB process_table[MAX_PROCS];

// global process queues
Queue *readyQueue;    // holds READY processes waiting for CPU
Queue *sleepQueue;    // holds BLOCKED processes waiting for Delay to expire

//Brk Location
void *current_kernel_brk;

//Kernel Tracking Logic
void *kernel_stack_limit;
UserContext *KernelUC;

//Frames available in physical memory {pmem_size / PAGESIZE}
unsigned long int frame_count;

//how many hardware clock ticks have occurred since boot
unsigned long current_tick = 0;

//Virtual Memory look up logic
// Something like -----> (vpn >= vp1) ? vpn - vp1 : vpn - vp0;
unsigned long int vp0 = VMEM_0_BASE >> PAGESHIFT;
unsigned long int vp1 = VMEM_1_BASE >> PAGESHIFT;

//Page Table allocation -> an array of page table entries
pte_t *kernel_page_table;

//User table needs to be private to each process

//Terminal Array
//Functions for terminal {TTY_TRANSMIT && TTY_RECEIVE}
static unsigned int terminal_array[NUM_TERMINALS];

//Global array for the Interrupt Vector Table
HandleTrapCall Interrupt_Vector_Table[TRAP_VECTOR_SIZE];

/* ==================================================================================================================
 * Kernel Stack Allocation function 
 * ==================================================================================================================
 */

//We dont need to call kernel s frame for idle process
//Idle process first 
//init second 
int create_sframes(PCB *free_proc, unsigned char *track, int track_size){
        TracePrintf(0, "Creating the Kernel Stack for the Process this many --> %d\n", KERNEL_STACK_PAGES);
        for(int i = 0; i < KERNEL_STACK_PAGES; i++){
                //Allocate a physical frame for kernel stack
		int pfn = find_frame(track, track_size);
                if(pfn == ERROR){
                        TracePrintf(0, "There is no more frames to give!\n");

                        //free the pcb itself
                        //pcb_free(pid);

                        return ERROR;
                }

                free_proc->kernel_stack_frames[i] = pfn;
        }
	return 0;
}

/* ==================================================================================================================
 * Proc Create Flow
 * This the most up to date and corrected version
 * ==================================================================================================================
 */

void idle_proc_create(unsigned char *track, int track_size, pte_t *user_page_table){
	//I have to malloc this proably 
	
	TracePrintf(1, "idle_proc_create(): begin\n");

	//Get a process from our PCB free list
	//idle_process = pcb_alloc();
	//if(idle_process == NULL){
	//	TracePrintf(0, "idle_proc_create(): ERROR pcb_alloc() returned NULL\n");
	//	return;
//	}

	//Malloc space for PCB idle struct 
	PCB *idle_process = (PCB *)malloc(sizeof(PCB));
	
	// -----------------------------------------------------------------------------------------------------------------
	
    	// Allocate a physical frame for the page table

	/*
   	int pt_pfn = frame_alloc(idle_process->pid);

	//WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
   	 if (pt_pfn == ERROR) {
   	     TracePrintf(0, "idle_proc_create(): ERROR allocating PT frame\n");
	     return;
   	 }

	 TracePrintf(1, "idle_proc_create(): PT frame pfn=%d\n\n", pt_pfn);
	
	 // -----------------------------------------------------------------------------------------------------------------

  	 // Find a free virtual page in kernel space to map this frame
	 int temp_vpn = -1;
	 unsigned long brk_page = ((unsigned long)current_kernel_brk) >> PAGESHIFT;

	 for (int i = brk_page; i < (KERNEL_STACK_BASE >> PAGESHIFT) - 1; i++) {
		 if (kernel_page_table[i].valid == FALSE) {

			 temp_vpn = i;
			 break;
		 }
	 }

    	if (temp_vpn < 0) {
		TracePrintf(0, "idle_proc_create(): ERROR no free kernel vpn for PT mapping\n");
		frame_free(pt_pfn);
		return;
    	}
  	
	//Map the pfn into the kernel_page_table so that it can be accessed by MMU
   	kernel_page_table[temp_vpn].pfn = pt_pfn;
    	kernel_page_table[temp_vpn].prot = PROT_READ | PROT_WRITE;
    	kernel_page_table[temp_vpn].valid = TRUE;

	TracePrintf(0, "Flushing Region 0 memory space so that it knows its updated\n");
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
	TracePrintf(0,"\n\n");

	*/

	// -----------------------------------------------------------------------------------------------------------------

	// Get pointer to the page table; we are getting the virtual address with temp_vpn << PAGESHIFT
	//Blueprint to talk to physical memory
	
	
	TracePrintf(0, "About to get idle_pt\n");
   	//pte_t *idle_pt = (pte_t *)(temp_vpn << PAGESHIFT); //this is proably wrong
	pte_t *idle_pt = user_page_table;
	
//	TracePrintf(0, "About to call memset on v_addr %p (vpn %d)\n", idle_pt, temp_vpn);
	memset(idle_pt, 0, sizeof(pte_t) * MAX_PT_LEN);

	//WE need to call helper_new_pid to inform the hardware
	//idle_process->pid = helper_new_pid(idle_pt);
	//TracePrintf(0, "Assigned idle_process->pid = %d\n", idle_process->pid);

	//Creats it kernel stack frames {Field inside of the struct PCB}
	//THIS SHOUD HAVE THE STACK FRAME SAVAIBLEW FOR IT 
	//if(create_sframes(idle_process, track, track_size) == ERROR){
	//	return;
//	}
	
	//TracePrintf(0,"\n\n");
	//TracePrintf(0,"We are going to alloc a frame the idle procs stack\n");

   //	int idle_stack_pfn = frame_alloc(idle_process->pid);

	//if (idle_stack_pfn == ERROR) {
	//	TracePrintf(0, "idle_proc_create(): ERROR allocating stack frame\n");
///		frame_free(pt_pfn);
		return;
//	}

//	TracePrintf(1, "idle_proc_create(): stack frame pfn=%d\n\n", idle_stack_pfn);

	//We are accessing region 1 page table
    //	unsigned long stack_page_index = MAX_PT_LEN - 1;
  //  	idle_pt[stack_page_index].valid = TRUE;
   // 	idle_pt[stack_page_index].prot = PROT_READ | PROT_WRITE;
   //	idle_pt[stack_page_index].pfn =  idle_stack_pfn;


//	WriteRegister(REG_PTBR1, (unsigned int)(temp_vpn << PAGESHIFT));
	WriteRegister(REG_PTBR1, (unsigned int)user_page_table);
	WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

	/* =======================
	 * idle_proc setup
	 * =======================
	 */

	//Use the PID helper function to get a pid
  	int pid_find = helper_new_pid(user_page_table);
	idle_process->pid = pid_find;
	TracePrintf(0, "Assigned idle_process->pid = %d\n", idle_process->pid);

	//idle_process->ppid = -1;

	//Store the byte address of the start of region 1 table
	//idle_process->AddressSpace = (void *)(uintptr_t)(temp_vpn << PAGESHIFT);
	idle_process->AddressSpace = user_page_table;
	
	//Set sp to the top of the user stack that we set up
	memcpy(&idle_process->curr_uc, KernelUC, sizeof(UserContext));

	idle_process->curr_uc.pc = (void*)DoIdle;
	idle_process->curr_uc.sp = (void*)(VMEM_1_LIMIT - 4);

	TracePrintf(0, "This is the value of the idle pc -- > %p and this is the value of the sp --> %p\n", idle_process->curr_uc.pc, idle_process->curr_uc.sp);
	TracePrintf(0, "idle_process->pid: %d\n", idle_process->pid);
	
	/* ======================================================
	 * Copy in idle PCB properties into the KernelUC
	 * ======================================================
	 */

	memcpy(KernelUC, &idle_process->curr_uc, sizeof(UserContext));

	/* =======================================
	 * Store AddressSpace for region 1 table
	 * =======================================
	 */

	//WriteRegister(REG_PTBR1, (unsigned int)(temp_vpn << PAGESHIFT));
	WriteRegister(REG_PTBR1, (unsigned int)user_page_table);
	WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
	
	//Flush for region 1 since we just wrote its start and limit
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

	//Set as running
	idle_process->currState = READY;

	//Set global variable for current process as the idle process
	current_process = idle_process;

	TracePrintf(0, "===+++++++++++++++++++++++++ IDLE PROCESS DEBUG +++++++++++++++++++++++++++++++++++++++++++====\n");
	TracePrintf(0, " This is the num of the array for the kernel_page_table --> %d", MAX_PT_LEN); 
	TracePrintf(0, "idle_process ptr =------------->>> : %p\n", idle_process);
	TracePrintf(0, "idle_process->pid: %d\n", idle_process->pid);
	TracePrintf(0, "This should be a reference to kernel page since the kernel does not interact with physical memory directly idle_process->AddressSpace: %p\n", idle_process->AddressSpace);
	TracePrintf(0, "This is the value of the VMEM_1_LIMIT ==> %p and this is the VMEM_1_BASE ==> %p\n", VMEM_1_LIMIT, VMEM_1_BASE);
//	TracePrintf(0, "pt_pfn: %d (0x%x)\n", pt_pfn, pt_pfn);
///	TracePrintf(0, "physical addr: 0x%lx\n", (unsigned long)(pt_pfn << PAGESHIFT));
	TracePrintf(0, "current_process ptr: %p\n", current_process);
	TracePrintf(0, "==========================\n");
	
	TracePrintf(0, "idle_proc_create(): done (pid=%d, aspace=%p)\n", idle_process->pid, idle_process->AddressSpace);
}

/* ==========================================================================================================================================================
 * Initializing Virtual Memory
 * char *cmd_args: Vector of strings, holding a pointer to each argc in boot command line {Terminated by NULL pointer}
 * unsigned int pmem_size: Size of the physical memory of the machine {Given in bytes}
 * UserContext *uctxt: pointer to an initial UserContext structure
 * ==========================================================================================================================================================
 */ 

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt){
	TracePrintf(0, "--------> We are the start of the KernelStart function <-----------");

	//Store the physical memory in global variable 
	pmem_size1 = pmem_size;

	//Calculate the number of page frames and store into our global variable 
	frame_count = (pmem_size / PAGESIZE);

	/* <<<---------------------------------------------------------
	 * Boot up the free frame data structure && define global vars
	 * ---------------------------------------------------------->>>
	 */

	unsigned char *track = (unsigned char*)malloc(frame_count);

	// Builds the frame table and marks kernel-reserved frames as used.
	init_frames(track, frame_count);

	//Set up the PCB table
	//InitPcbTable();	

	// initialize process queues
	readyQueue = initializeQueue();
	sleepQueue = initializeQueue();

	//Fill in KernelUC to be able to pass current User Context around 
	KernelUC = uctxt;

	/* <<<---------------------------------------
	 * Set up the initial Region 0 {KERNEL SPACE}
	 * Build Region 0 mappings for kernel text, data, heap, and stack.  
	 * -->Stack	-
	 *  		-
	 *  		-
	 * --> Heap	-
	 * --> Data	-
	 *  -->Text	-
	 * --------------------------------------->>>
	 */

	//Malloc region 0 page table memory
	//Number of Page Tables * the size of the its struct 
	
	kernel_page_table = malloc(MAX_PT_LEN * sizeof(pte_t));
	pte_t *user_page_table = malloc(MAX_PT_LEN * sizeof(pte_t));

	//Call function to set it up 
	SetupRegion0(kernel_page_table);

	SetupRegion1(user_page_table);

	/* <<<--------------------------
	 * Initialize Virtual Memory
	 * -------------------------->>>
	 */
	
	TracePrintf(1, "------------------------------------- WE ARE TURNING ON VIRTUAL MEMORY NOW :) -------------------------------------\n\n");
	WriteRegister(REG_VM_ENABLE, TRUE);
	vm_enabled = TRUE;
	TracePrintf(1, "##################################### VIRTUAL MEMORY SECTION IS DONE ################################################\n\n");

	/* <<<------------------------------
	 * Set up the Interrupt Vector Table
	 * ------------------------------>>>
	 */

	// Each trap/interrupt is linked to its handler so hardware can dispatch correctly.
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

	/* <<<-------------------------------------
	 * Create Process
	 *The idle proces
	 * ------------------------------------->>>
	*/

	//Create the idle proc or process 1
	idle_proc_create(track, frame_count, kernel_page_table);
	
	/*
	TracePrintf(1, "KernelStart: creating the init process ======================================================>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	TracePrintf(1, "We are going into the CreateInit process ++++\n");

	PCB *init_pcb = createInit();
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

	*/


	TracePrintf(1, "KernelStart complete.\n");
	return;
}
