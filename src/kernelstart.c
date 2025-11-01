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
PCB process_table[MAX_PROCS];

// global process queues
Queue *readyQueue;    // holds READY processes waiting for CPU
Queue *sleepQueue;    // holds BLOCKED processes waiting for Delay to expire

//Brk Location
void *current_kernel_brk;

//Kernel Tracking Logic
void *kernel_region_pt;
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
pte_t kernel_page_table[MAX_PT_LEN];

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
int create_sframes(int pid, PCB *free_proc){
        TracePrintf(0, "Creating the Kernel Stack for the Process this many --> %d\n", KERNEL_STACK_PAGES);

        for(int i = 0; i < KERNEL_STACK_PAGES; i++){
                //Allocate a physical frame for kernel stack
                int pfn = frame_alloc(pid);
                if(pfn == ERROR){
                        TracePrintf(0, "We ran out of frames to give!\n");

                        //Unmap any previous frames that we allocated
                        for(int f = 0; f < i; f++){
                                frame_free(free_proc->kernel_stack_frames[f]);
                        }

                        //free the pcb itself
                        pcb_free(pid);

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

void idle_proc_create(void){

	TracePrintf(1, "idle_proc_create(): begin\n");

	//Get a process from our PCB free list
	idle_process = pcb_alloc();
	if(idle_process == NULL){
		TracePrintf(0, "idle_proc_create(): ERROR pcb_alloc() returned NULL\n");
		return;
	}
	
	// -----------------------------------------------------------------------------------------------------------------

  	/* =======================
    	 * Allocate a new page table for idle process
     	 * =======================
     	 */

	idle_process->pid = 0;

    	// Allocate a physical frame for the page table
	TracePrintf(0, "Calling the frame_alloc function to be able to map a physical frame for our idle process\n");
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

	 for (int i = brk_page; i < (KERNEL_STACK_BASE >> PAGESHIFT) - 10; i++) {
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

	// -----------------------------------------------------------------------------------------------------------------

	// Get pointer to the page table; we are getting the virtual address with temp_vpn << PAGESHIFT
	//Blueprint to talk to physical memory
	TracePrintf(0, "About to get idle_pt\n");
   	pte_t *idle_pt = (pte_t *)(temp_vpn << PAGESHIFT);
	
	TracePrintf(0, "About to call memset on v_addr %p (vpn %d)\n", idle_pt, temp_vpn);

	helper_check_heap("before");
	//memset(idle_pt, 0, sizeof(pte_t) * MAX_PT_LEN);
	helper_check_heap("after");

	//WE need to call helper_new_pid to inform the hardware
	idle_process->pid = helper_new_pid(idle_pt);
	TracePrintf(0, "Assigned idle_process->pid = %d\n", idle_process->pid);

	//Creats it kernel stack frames {Field inside of the struct PCB}
	if(create_sframes(idle_process->pid, idle_process) == ERROR){
		return;
	}
	
	TracePrintf(0,"\n\n");
	TracePrintf(0,"We are going to alloc a frame the idle procs stack\n");
   	int idle_stack_pfn = frame_alloc(idle_process->pid);

	if (idle_stack_pfn == ERROR) {
		TracePrintf(0, "idle_proc_create(): ERROR allocating stack frame\n");
		frame_free(pt_pfn);
		return;
	}

	TracePrintf(1, "idle_proc_create(): stack frame pfn=%d\n\n", idle_stack_pfn);
    	
	//We are accessing region 1 page table
    	unsigned long stack_page_index = MAX_PT_LEN - 1;
    	idle_pt[stack_page_index].valid = TRUE;
    	idle_pt[stack_page_index].prot = PROT_READ | PROT_WRITE;
   	idle_pt[stack_page_index].pfn =  idle_stack_pfn;

	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

	/* =======================
	 * idle_proc setup
	 * =======================
	 */

	TracePrintf(0, "Assigned idle_process->pid = %d\n", idle_process->pid);
	idle_process->ppid = -1;

	//Store the byte address of the start of region 1 table
	idle_process->AddressSpace = (void *)(uintptr_t)(temp_vpn << PAGESHIFT);
	
	//Set sp to the top of the user stack that we set up
	memcpy(&idle_process->curr_uc, KernelUC, sizeof(UserContext));

	idle_process->curr_uc.pc = (void*)DoIdle;
	idle_process->curr_uc.sp = (void*)(VMEM_1_LIMIT - 1);

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

	WriteRegister(REG_PTBR1, (unsigned int)(temp_vpn << PAGESHIFT));
	WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
	
	//Flush for region 1 since we just wrote its start and limit
	WriteRegister(REG_TLB_FLUSH, (unsigned int)temp_vpn << PAGESHIFT);

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
	TracePrintf(0, "pt_pfn: %d (0x%x)\n", pt_pfn, pt_pfn);
	TracePrintf(0, "physical addr: 0x%lx\n", (unsigned long)(pt_pfn << PAGESHIFT));
	TracePrintf(0, "current_process ptr: %p\n", current_process);
	TracePrintf(0, "==========================\n");
	
	TracePrintf(0, "idle_proc_create(): done (pid=%d, aspace=%p)\n", idle_process->pid, idle_process->AddressSpace);
}


/* ==============================================================================================
 * SetKernelBrk Function Logic
 * ==============================================================================================
 */ 

int SetKernelBrk(void * addr){
	TracePrintf(1, "We are in the function SetKernelBrk\n");

	//Convert the address to uintptr_t to be able to use as a integer
	uintptr_t new_kbrk_addr = (uintptr_t)addr;
	uintptr_t old_kbrk = (uintptr_t)current_kernel_brk;

	/* SetKernelBrk operates differently depending on if virtual memory is enabled or not 
	 * Before Virtual Memory is enabled, it checks if and by how much the SetKernelBrk is being raised from the original kernel check point
	 */
	
	if(vm_enabled == FALSE){
		TracePrintf(1, "THIS IS CALLED WHEN VIRTUAL MEMORY IS NOT ENABLED\n");	

		//Normalize byte address for brk_addr
		uintptr_t original_brk_addr = (uintptr_t)_orig_kernel_brk_page * PAGESIZE;

		//It can not be less then the current break point since this space is used for data
		if(new_kbrk_addr < old_kbrk){
			TracePrintf(0, "You can not shrink the heap!\n");
			return ERROR;
		}

		uintptr_t heap_start = (uintptr_t)_orig_kernel_brk_page * PAGESIZE;
		uintptr_t heap_end_limit = (uintptr_t)KERNEL_STACK_BASE;

		// can't shrink below data/heap start
		if (new_kbrk_addr < heap_start) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p is below kernel heap start (%p)\n", (void*)new_kbrk_addr, (void*)heap_start);
		    return ERROR;
		}

		// can't grow into kernel stack region
		if (new_kbrk_addr >= heap_end_limit) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p would overlap kernel stack (%p)\n", (void*)new_kbrk_addr, (void*)heap_end_limit);
		    return ERROR;
		}

		uintptr_t phys_limit = frame_count * PAGESIZE;
		if (new_kbrk_addr >= phys_limit) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p exceeds physical memory limit (%p)\n",
		                (void*)new_kbrk_addr, (void*)phys_limit);
		    return ERROR;
		}

		current_kernel_brk = (void *)new_kbrk_addr;

		TracePrintf(1, "[SetKernelBrk] Updated current_brk (no VM): %p\n", current_kernel_brk);
        	return 0;
	}
	else if (vm_enabled == TRUE){
		TracePrintf(1, "VIRTUAL MEMORY HAS BEEN ENABLED \n");

		//SetKernelBrk functions as regular brk after Virtual Memory has been initialized
		//Stores the byte address of the kernel break
		uintptr_t original_brk_addr = (uintptr_t)_orig_kernel_brk_page * PAGESIZE;

		if(new_kbrk_addr < original_brk_addr){
			TracePrintf(0, "Error! I will be writing into the data section!");
			return ERROR;
		}
		
		//Check if the requested new address space for the Kernel Heap Brk is valid

		uintptr_t heap_start = (uintptr_t)_orig_kernel_brk_page * PAGESIZE;
		uintptr_t heap_end_limit = (uintptr_t)KERNEL_STACK_BASE;


		TracePrintf(1, "[SetKernelBrk-VM] Comparing new_brk: %p against heap_start: %p\n",
                    (void*)new_kbrk_addr, (void*)heap_start);


		// 1. must not go below heap start (data section)
		if (new_kbrk_addr < heap_start) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p is below kernel heap start (%p)\n", (void*)new_kbrk_addr, (void*)heap_start);
		    return ERROR;
		}

		// 2. must not grow into kernel stack
		if (new_kbrk_addr >= heap_end_limit) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p overlaps kernel stack base (%p)\n", (void*)new_kbrk_addr, (void*)heap_end_limit);
		    return ERROR;
		}

		uintptr_t grow_start = UP_TO_PAGE(old_kbrk);        // first new page if growing
		uintptr_t grow_end   = UP_TO_PAGE(new_kbrk_addr);   // one past last page
							
		// Step 2: Growing the heap (allocate frames)
		if (grow_end > grow_start) {
		        TracePrintf(1, "[SetKernelBrk] Growing kernel heap\n");

		        uintptr_t mapped_until = grow_start;

		        for (uintptr_t vaddr = grow_start; vaddr < grow_end; vaddr += PAGESIZE) {

		                int vpn = (int)(vaddr >> PAGESHIFT);

		                // skip if already valid 
		                if (kernel_page_table[vpn].valid) {
		                        mapped_until = vaddr + PAGESIZE;
		                        continue;
		                }

		                int pfn = frame_alloc(0);

		                if (pfn < 0) {
		                        // roll back anything we just mapped
		                        for (uintptr_t rv = grow_start; rv < mapped_until; rv += PAGESIZE) {
		                                int rvpn = (int)(rv >> PAGESHIFT);
		                                if (kernel_page_table[rvpn].valid) {
		                                        frame_free(kernel_page_table[rvpn].pfn);
		                                        kernel_page_table[rvpn].valid = FALSE;
		                                        kernel_page_table[rvpn].prot  = 0;
		                                        kernel_page_table[rvpn].pfn   = 0;
		                                }
		                        }
					WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
		                        TracePrintf(0, "[SetKernelBrk] Out of frames while growing!\n");
		                        return ERROR;
		                }

		                kernel_page_table[vpn].pfn   = pfn;
		                kernel_page_table[vpn].prot  = PROT_READ | PROT_WRITE;
		                kernel_page_table[vpn].valid = TRUE;

		                mapped_until = vaddr + PAGESIZE;
			        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

		        }

		      //  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
		}
		else if (grow_end < grow_start) {
		        TracePrintf(1, "[SetKernelBrk] Shrinking kernel heap...\n");
		        for (uintptr_t vaddr = grow_end; vaddr < grow_start; vaddr += PAGESIZE) {
		                int vpn = (int)(vaddr >> PAGESHIFT);
		                if (kernel_page_table[vpn].valid) {
		                        frame_free(kernel_page_table[vpn].pfn);
		                        kernel_page_table[vpn].valid = FALSE;
		                        kernel_page_table[vpn].prot  = 0;
		                        kernel_page_table[vpn].pfn   = 0;
		                }
		        }

		        // flush TLB for region 0
		        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
		}
		 // Step 4: Finalize and return
	        // ------------------------------------------------------------
	        current_kernel_brk = (void *)new_kbrk_addr;
	        TracePrintf(1, "[SetKernelBrk] Moved break to %p (VM enabled)\n", current_kernel_brk);
	        return 0;
	    }
	TracePrintf(0, "[SetKernelBrk] Unknown VM state!\n");
	return ERROR;
}

/* ==========================================================================================================================================================
 * Initializing Virtual Memory
 * char *cmd_args: Vector of strings, holding a pointer to each argc in boot command line {Terminated by NULL pointer}
 * unsigned int pmem_size: Size of the physical memory of the machine {Given in bytes}
 * UserContext *uctxt: pointer to an initial UserContext structure
 * ==========================================================================================================================================================
 */ 

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt){

	pmem_size1 = pmem_size;

	//Calculate the number of page frames and store into our global variable 
	frame_count = (pmem_size / PAGESIZE);
	TracePrintf(1, "\n\n");
	TracePrintf(1,"========> This is the start of the KernelStart function <========\n");
	TracePrintf(1, "<<<<<<<<<<<<<<<< this is the size pmem_size => %lX and this is the stack frames ==> %d >>>>>>>>>>>>>>>>>>>>>>>>\n", pmem_size, frame_count);

	/* <<<---------------------------------------------------------
	 * Boot up the free frame data structure && define global vars
	 * ---------------------------------------------------------->>>
	 */

	//Set up the PCB table
	InitPcbTable();	

	// initialize process queues
	readyQueue = initializeQueue();
	sleepQueue = initializeQueue();

	//Set up global variable for the current UserContext
	KernelUC = uctxt;

	/* <<<------------------------------
	 * Set up the Interrupt Vector Table
	 * ------------------------------>>>
	 */

	// Each trap/interrupt is linked to its handler so hardware can dispatch correctly.
	TracePrintf(1,"+++++ We are setting up the IVT and im going to call the function | CALLED KERNELSTART \n");
	setup_trap_handler(Interrupt_Vector_Table);
	TracePrintf(1,"+++++ We have left the function and are now going to set up region 0 | CALLED KERNELSTART\n\n");

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

	/*Loop through the text section and set up the ptes
	 * #include <yalnix.h>
	 * _first_kernel_text_page - > low page of kernel text
	 */

	TracePrintf(1,"------------------------------------------------- Region 0 Set up ----------------------------------------------------\n");

	//Set the Global variable
	kernel_region_pt = (void *)kernel_page_table;
	TracePrintf(1,"This is where my array is located --> %p\n", kernel_region_pt);
	
	unsigned long int text_start = _first_kernel_text_page;
	unsigned long int text_end = _first_kernel_data_page;
	TracePrintf(1," this is the value of text_start --> %lu and this is text_end --> %lu \n", text_start, text_end);

	for(unsigned long int text = text_start; text < text_end; text++){
		//Text section should be only have READ && EXEC permissions
		kernel_page_table[text].prot = PROT_READ | PROT_EXEC;
		kernel_page_table[text].valid = TRUE;
		kernel_page_table[text].pfn = text;
	}

	/*Loop through heap and data sections and setup pte
	 * _first_kernel_data_page -> low page of heap/data section
	 * _orig_kernel_brk_page -> first unused page after kernel heap
	 */

	unsigned long int heapdata_start = _first_kernel_data_page; 
	unsigned long int heapdata_end = _orig_kernel_brk_page;
	TracePrintf(1," this is the value of heapdata_start --> %lu and this is heapdata_end --> %lu \n", heapdata_start, heapdata_end);
	for(unsigned long int data_heap = heapdata_start; data_heap < heapdata_end; data_heap++){
		//Heap and Data section both have READ and WRITE conditions
		kernel_page_table[data_heap].prot = PROT_WRITE | PROT_READ; 
		kernel_page_table[data_heap].valid = TRUE;
		kernel_page_table[data_heap].pfn = data_heap;
	}
	
	TracePrintf(1,"This is the redzone and we neeed to print this out\n");
	/* ==============================
	 * Red Zone {Unmapped Pages}
	 * ==============================
	 */

	unsigned long int stack_start = KERNEL_STACK_BASE >> PAGESHIFT;
	unsigned long int stack_end = KERNEL_STACK_LIMIT >> PAGESHIFT;
	
	TracePrintf(1,"This is in the stack loop logic\n");
	TracePrintf(1," this is the value of stack_start --> %lu and this is stackend --> %lu \n", stack_start, stack_end);

	for(unsigned long int stack_loop = stack_start; stack_loop < stack_end; stack_loop++){
		kernel_page_table[stack_loop].prot = PROT_READ | PROT_WRITE;
		kernel_page_table[stack_loop].valid = TRUE;
		kernel_page_table[stack_loop].pfn = stack_loop; 
	}

	//Set global variable for stack limit for region 0
	kernel_stack_limit = (void *)KERNEL_STACK_LIMIT;
	
	TracePrintf(1, "This is the base of the kernel_page_table ===> %p\n", kernel_page_table);
	//Write the address of the start of text for pte_t
	WriteRegister(REG_PTBR0,(unsigned int)kernel_page_table);

	//MAX_PT_LEN because REG_PTLR0 needs number of entries in the page table for region 0
	WriteRegister(REG_PTLR0, (unsigned int)MAX_PT_LEN);

	TracePrintf(1,"############################################## Region 0 Setup Done #############################################################\n\n");

	/* <<<--------------------------
	 * Initialize Virtual Memory
	 * -------------------------->>>
	 */
	
	TracePrintf(1, "------------------------------------- WE ARE TURNING ON VIRTUAL MEMORY NOW :) -------------------------------------\n\n");
	WriteRegister(REG_VM_ENABLE, TRUE);
	vm_enabled = TRUE;
	TracePrintf(1, "##################################### VIRTUAL MEMORY SECTION IS DONE ################################################\n\n");

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

	TracePrintf(1, "##################################### WE HAVE CALLED KERNELBREAK ##################################################\n\n");


	TracePrintf(1, "------------------------------------- TIME TO CREATE OUR PROCESS-------------------------------------\n");

	// Builds the frame table and marks kernel-reserved frames as used.
	frames_init(pmem_size);

	/* <<<-------------------------------------
	 * Create Process
	 *The idle proces
	 * ------------------------------------->>>
	*/

	//Create the idle proc or process 1
	idle_proc_create();

	TracePrintf(1, "KernelStart: creating the init process ======================================================>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

//	PCB *init_pcb = createInit();
	//if(init_pcb == NULL){
//		TracePrintf(0, "There was an error when trying to call pcb_alloc for init process");
//		Halt();
//	}


	if(cmd_args[0] == NULL){
		TracePrintf(0 ,"No argument was passed! Calling the init default function\n");
		//init();
	}

	/*
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
