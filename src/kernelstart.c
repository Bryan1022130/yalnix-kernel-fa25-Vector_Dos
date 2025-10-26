//Header files from yalnix_framework && libc library
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h> //For u_long

#include <ctype.h> // <----- NOT USED RIGHT NOW ----->
#include <load_info.h> //The struct for load_info
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ... 
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions 
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

/*
 * ================================>>
 * Global Variables
 * <<================================
 */

//Virtual Memory Check
short int vm_enabled = FALSE;

//Process Block
PCB *current_process = NULL; 
PCB *idle_process = NULL;
PCB *process_ready_head = NULL;
PCB process_table[MAX_PROCS];

//Brk Location
void *current_kernel_brk;

//Kernel Tracking Logic
void *kernel_region_pt;
void *kernel_stack_limit;
UserContext *KernelUC;

//Frames available in physical memory {pmem_size / PAGESIZE}
unsigned long int frame_count;

//Virtual Memory look up logic
// Something like -----> (vpn >= vp1) ? vpn - vp1 : vpn - vp0;
unsigned long int vp0 = VMEM_0_BASE >> PAGESHIFT;
unsigned long int vp1 = VMEM_1_BASE >> PAGESHIFT;

//Page Table allocation -> an array of page table entries
static pte_t kernel_page_table[MAX_PT_LEN];
static pte_t user_page_table[MAX_PT_LEN];

//Terminal Array
//Functions for terminal {TTY_TRANSMIT && TTY_RECEIVE}
static unsigned int terminal_array[NUM_TERMINALS];

//Global array for the Interrupt Vector Table
HandleTrapCall Interrupt_Vector_Table[TRAP_VECTOR_SIZE];

/* =======================================
 * Idle Function that runs in Kernel Space
 * =======================================
 */

void DoIdle(void) { 
	while(1) {
		TracePrintf(1,"DoIdle\n");
		Pause();
	}
}

/* =======================================
 * Process Logic Functions
 * =======================================
 */

void InitPcbTable(void){
    // Zero out the entire array
    memset(process_table, 0, sizeof(process_table));

    // Set all entries in the pcb table as free 
    for (int i = 0; i < MAX_PROCS; i++) {
        process_table[i].currState = FREE;
    }

    TracePrintf(1, "PCB table initialized \n");
}


PCB *pcb_alloc(void){
	for (int pid = 0; pid < MAX_PROCS; pid++) {
		if (process_table[pid].currState == FREE) {

			//Clear out the data from prev processes
			memset(&process_table[pid], 0, sizeof(PCB));

			process_table[pid].currState = READY; 
			process_table[pid].pid = pid;

			return &process_table[pid];
		}
	}
	
	TracePrintf(0, "ERROR! No free PCBs available.\n");
	return NULL;
}

int pcb_free(int pid){

    	if(pid < 0 || pid >= MAX_PROCS){ 
        	TracePrintf(0, "This is a invalid pid");
		return -1;
	}

	if(process_table[pid].currState == FREE){
		TracePrintf(0, "Your process is already free");
		return -1;
	}

	PCB *proc = &process_table[pid];

	pte_t *pt_r1 = (pte_t *)proc->AddressSpace;

	if (pt_r1 != NULL) {
		//Index to be able to loop through each pte and free 
		int num_r1_pages = (VMEM_1_SIZE >> PAGESHIFT);

		for (int vpn = 0; vpn < num_r1_pages; vpn++) {
			if (pt_r1[vpn].valid) {
				frame_free(pt_r1[vpn].pfn);
           		}
		}
	
	//Free the page table
	uintptr_t pt_vaddr = (uintptr_t)pt_r1;

	process_table[pid].currState = FREE;

	//Get the pfn for the page table inside of Region 0
	int pt_vpn_r0 = (int)(pt_vaddr >> PAGESHIFT);
        int pt_pfn = kernel_page_table[pt_vpn_r0].pfn;

        // Free the physical frame that held the page table
        frame_free(pt_pfn);
	
	//Mark the virtual mapping as invalid for physical memmory
	kernel_page_table[pt_vpn_r0].valid = FALSE;
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

	memset(proc, 0, sizeof(PCB));

	}
	memset(proc, 0, sizeof(PCB));
	proc->currState = FREE;

	TracePrintf(0, "I have freed your process from the proc table :)");	
	return 0;

}

void init_proc_create(void){

	//Get a process from our PCB free list
	idle_process = pcb_alloc();

	if(idle_process == NULL){
		TracePrintf(0, "There was an error when trying with pcb_alloc, NULL returned!");
		return;
	}
	
	/* =======================
	 * Pid Logic
	 * =======================
	 */
	
	//Get a pid for the process
	idle_process->pid = helper_new_pid(user_page_table);

	//To indicate that its the kernel process itself
	idle_process->ppid = 0;
	
	/* ======================================================
	 * Copy in UserContext from KernelStart into the idle PCB
	 * ======================================================
	 */

	memcpy(&idle_process->curr_uc, KernelUC, sizeof(UserContext));
	
	/* =======================================
	 * Store AddressSpace for region 1 table
	 * =======================================
	 */

	idle_process->AddressSpace = user_page_table;
	
	//Set the pc to DoIdle location
	//Set sp to the top of the user stack that we set up
	idle_process->curr_uc.pc = (void*)DoIdle;
	idle_process->curr_uc.sp = (void*)(VMEM_1_LIMIT - PAGESIZE);

	//Set as running
	idle_process->currState = READY;
	
	//Track Kernel Stack Frames
	

	//Set global variable for current process as the idle process
	current_process = idle_process;

	memcpy(KernelUC, &idle_process->curr_uc, sizeof(UserContext));

}

/* ========================================================
 * SetKernelBrk Function Logic
 * ========================================================
 */ 

int SetKernelBrk(void * addr){
	//Convert the address to uintptr_t to be able to use as a integer
	uintptr_t new_kbrk_addr = (uintptr_t)addr;
	uintptr_t old_kbrk = (uintptr_t)current_kernel_brk;

	/* SetKernelBrk operates differently depending on if virtual memory is enabled or not 
	 * Before Virtual Memory is enabled, it checks if and by how much the SetKernelBrk is being raised from the original kernel check point
	 */
	
	if(vm_enabled == FALSE){
		uintptr_t original_brk_addr = (uintptr_t)_orig_kernel_brk_page * PAGESIZE;

		//It can not be less then the original break point since this space is used for data
		if(new_kbrk_addr < old_kbrk){
			TracePrintf(0, "You can not shrink the heap!");
			return ERROR;
		}

		//Here check for more conditions that would be invalid, like if the new_kbrk_addr were to be greater than memory there is
		// --- address validity checks before VM is enabled ---
		uintptr_t heap_start = (uintptr_t)_orig_kernel_brk_page;
		uintptr_t heap_end_limit = (uintptr_t)KERNEL_STACK_BASE;

		// can't shrink below data/heap start
		if (new_kbrk_addr < heap_start) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p is below kernel heap start (%p)\n",
		                (void*)new_kbrk_addr, (void*)heap_start);
		    return ERROR;
		}

		// can't grow into kernel stack region
		if (new_kbrk_addr >= heap_end_limit) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p would overlap kernel stack (%p)\n",
		                (void*)new_kbrk_addr, (void*)heap_end_limit);
		    return ERROR;
		}

		uintptr_t phys_limit = frame_count * PAGESIZE;
		if (new_kbrk_addr >= phys_limit) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p exceeds physical memory limit (%p)\n",
		                (void*)new_kbrk_addr, (void*)phys_limit);
		    return ERROR;
		}

		// 4. warn if not page aligned
		if ((new_kbrk_addr & PAGEOFFSET) != 0) {
		    TracePrintf(1, "[SetKernelBrk] Warning: unaligned address %p; rounding internally.\n",
		                (void*)new_kbrk_addr);
		}

		current_kernel_brk = (void *)new_kbrk_addr;
		TracePrintf(1, "[SetKernelBrk] Updated current_brk (no VM): %p\n", current_kernel_brk);
        	return 0;
	}
	else if (vm_enabled == TRUE){
		//SetKernelBrk functions as regular brk after Virtual Memory has been initialized
		
		//Stores the byte address of the kernel break
		uintptr_t original_brk_addr = (uintptr_t)_orig_kernel_brk_page;

		if(new_kbrk_addr < original_brk_addr){
			TracePrintf(0, "Error! I will be writing into the data section!");
			return ERROR;
		}
		
		//Check if the requested new address space for the Kernel Heap Brk is valid

		// --- address validity checks ---
		uintptr_t heap_start = (uintptr_t)_orig_kernel_brk_page;
		uintptr_t heap_end_limit = (uintptr_t)KERNEL_STACK_BASE;

		// 1. must not go below heap start (data section)
		if (new_kbrk_addr < heap_start) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p is below kernel heap start (%p)\n",
		                (void*)new_kbrk_addr, (void*)heap_start);
		    return ERROR;
		}

		// 2. must not grow into kernel stack
		if (new_kbrk_addr >= heap_end_limit) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p overlaps kernel stack base (%p)\n",
		                (void*)new_kbrk_addr, (void*)heap_end_limit);
		    return ERROR;
		}

		// 3. warn if not page aligned
		if ((new_kbrk_addr & PAGEOFFSET) != 0) {
		    TracePrintf(1, "[SetKernelBrk] Warning: unaligned address %p; rounding to page boundary.\n",
		                (void*)new_kbrk_addr);
		}

		//1.We can now call a function that will set up the page table entries for the new allocated space
		//2.We have to calculate the page-algned memory space to be able to loop
		//3.We would have to loop from the start of the old break pointer to the new one (By page size and not actual address value)
		//4.Try to allocate a new space in physical memory to map the virtual address to
		//5.On each iteration we need to set up the table entries with correct permissions and information for virtual memory
		
		//6.Perform other forms of error checking related to the addr passed in 

		// Step 1: Align old/new break values
		// ---------------------------------------------------------
		uintptr_t grow_start = UP_TO_PAGE(old_kbrk);        // first new page if growing
		uintptr_t grow_end   = UP_TO_PAGE(new_kbrk_addr);   // one past last page
							
		// If grow_end > grow_start → we’re expanding the heap
		// If grow_end < grow_start → we’re shrinking

		// Step 2: Growing the heap (allocate frames)
		if (grow_end > grow_start) {
		        TracePrintf(1, "[SetKernelBrk] Growing kernel heap...\n");
		        uintptr_t mapped_until = grow_start;   // track how far we’ve gotten

		        for (uintptr_t vaddr = grow_start; vaddr < grow_end; vaddr += PAGESIZE) {
		                int vpn = (int)(vaddr >> PAGESHIFT);

		                // skip if already valid 
		                if (kernel_page_table[vpn].valid) {
		                        mapped_until = vaddr + PAGESIZE;
		                        continue;
		                }

		                int pfn = frame_alloc(-1);   // -1 marks kernel ownership
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
		                        TracePrintf(0, "[SetKernelBrk] Out of frames while growing!\n");
		                        return ERROR;
		                }

		                kernel_page_table[vpn].pfn   = pfn;
		                kernel_page_table[vpn].prot  = PROT_READ | PROT_WRITE;
		                kernel_page_table[vpn].valid = 1;

		                mapped_until = vaddr + PAGESIZE;
		        }

		        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
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
	return -1;
}

/* ========================================================
 * Initializing Virtual Memory
 * char *cmd_args: Vector of strings, holding a pointer to each argc in boot command line {Terminated by NULL poointer}
 * unsigned int pmem_size: Size of the physical memory of the machine {Given in bytes}
 * UserContext *uctxt: pointer to an initial UserContext structure
 * =========================================================
 */ 

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt){


	TracePrintf(0,"WE ARE CURRENTLY IN THE KERNELSTART PROGRAM\n");

	TracePrintf(0, "<<<<<<<<<<<<<<<< this is the size og pmem_size => %lx", pmem_size);

	/* <<<---------------------------------------------------------
	 * Boot up the free frame data structure && definee global vars
	 * ---------------------------------------------------------->>>
	 */

	//Calculate the number of page frames and store into our global variable 
	frame_count = pmem_size / PAGESIZE;
	
	//Initialize the data struct to track the free frames
	frames_init(pmem_size);

	//Set up global variable for UserContext
	KernelUC = uctxt;

	/* <<<------------------------------
	 * Set up the Interrupt Vector Table
	 * ------------------------------>>>
	 */
	
	TracePrintf(0,"WE ARE SETTING UP THE INTERRUPTM VERCTOR TABLE\n");
	setup_trap_handler(Interrupt_Vector_Table);


	/* <<<---------------------------------------
	 * Set up the initial Region 0 {KERNEL SPACE}
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
	
	TracePrintf(0,"WE ARE WRITING THE BASE REGISTER FOR REGION 0\n");

	//Set the Global variable
	kernel_region_pt = (void *)kernel_page_table;


	
	unsigned long int text_start = _first_kernel_text_page;
	TracePrintf(0, "This is the value of the text_page --> %lx and this is the of with the DOWN_TO_PAGE --> %lx +++++++++++++\n", _first_kernel_text_page, text_start);

	unsigned long int text_end = _first_kernel_data_page;

	TracePrintf(0, "This is the value of the text_end ---> %lx\n ++++++++++++++", text_end);
	
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
	TracePrintf(0, "This is the value of the heapdata_start---> %lx\n ++++++++++++++++", heapdata_start);

	unsigned long int heapdata_end = _orig_kernel_brk_page;
	TracePrintf(0, "This is the value of the heapdata_end ---> %lx\n ++++++++++++++", heapdata_end);

	for(unsigned long int data_heap = heapdata_start; data_heap < heapdata_end; data_heap++){
		//Heap and Data section both have READ and WRITE conditions
		kernel_page_table[data_heap].prot = PROT_WRITE | PROT_READ; 
		kernel_page_table[data_heap].valid = TRUE;
		kernel_page_table[data_heap].pfn = data_heap;
	}
	TracePrintf(0,"WE ARE GETTING IN THE RED ZONE PAGE\n");


	/* ==============================
	 * Red Zone {Unmapped Pages}
	 * ==============================
	 */

	TracePrintf(0,"WE ARE GETTING IN THE RED ZONE PAGE\n");

	unsigned long int stack_start = KERNEL_STACK_BASE >> PAGESHIFT;
	unsigned long int stack_end = KERNEL_STACK_LIMIT >> PAGESHIFT;
	
	TracePrintf(0,"This is in the stack loop\n");


	TracePrintf(0," this is the value of stack_start --> %lx and this is stackend --> %lx \n", stack_start, stack_end);

	for(unsigned long int stack_loop = stack_start; stack_loop < stack_end; stack_loop++){

		TracePrintf(0,"WE ARE in the stack loop\n");
		kernel_page_table[stack_loop].prot = PROT_READ | PROT_WRITE;
		kernel_page_table[stack_loop].valid = TRUE;
		kernel_page_table[stack_loop].pfn = stack_loop; 
	}

	TracePrintf(0,"THIS IS BEFORE WE ARE WRITING THE PAGE TABLE ADRESSSSSSSSS\n");

	//Set global variable for stack limit
	kernel_stack_limit = (void *)VMEM_0_LIMIT;

	//Write the address of the start of text for pte_t
	WriteRegister(REG_PTBR0,(unsigned int)kernel_page_table);
	
	//Write the page table table limit register for Region 0
	//MAX_PT_LEN because REG_PTLR0 needs number of entries in the page table for region 0
	WriteRegister(REG_PTLR0, (unsigned int)MAX_PT_LEN);

	TracePrintf(0,"WE ARE DONE SETTTING UP THE PAGE TABLES FOR STACK, TEXT AND DATA\n");

	/* <<<-----------------------------------
	 * Set Up Region 1 for the Idle Process
	 * ----------------------------------->>>
	 */

	//Already allocated reginn 1 Page table as global
	
	int idle_stack_pfn = frame_alloc(0);

	if (idle_stack_pfn == -1) {
		TracePrintf(0, "FAILED to allocate frame for idle stack!\n");
		return;
	}

	unsigned long stack_page_index = MAX_PT_LEN - 1;

   	 user_page_table[stack_page_index].valid = 1;
   	 user_page_table[stack_page_index].prot = PROT_READ | PROT_WRITE;
   	 user_page_table[stack_page_index].pfn = idle_stack_pfn;


	//Write the limit of the region 1 memory
	//Write the base of the region 1 memory
	WriteRegister(REG_PTBR1, (unsigned int)user_page_table);
	WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);


	/* <<<--------------------------
	 * Initialize Virtual Memory
	 * -------------------------->>>
	 */
		
	//Write to special register that Virtual Memory is enabled 
	WriteRegister(REG_VM_ENABLE, TRUE);

	//Set the global variable as true 
	vm_enabled = TRUE;

	/* <<<------------------------------
	 * Call SetKernelBrk()
	 * ------------------------------>>>
	 */

	//Set current brk and then call SetKernelBrk
	
    	uintptr_t orig_brk_address = (uintptr_t)_orig_kernel_brk_page * PAGESIZE;

	current_kernel_brk = (void *)orig_brk_address;

	int kbrk_return = SetKernelBrk(current_kernel_brk);

	if(kbrk_return != 0){
		TracePrintf(0, "There was an error in SetKernelBrk");
		return;
	}

	/* <<<-------------------------------------
	 * Create Process
	 * ------------------------------------->>>
	 */

	//Create idle proc
	InitPcbTable();
	init_proc_create();

	TracePrintf(0, "I am leaving KernelStart\n");
	TracePrintf(0, "Good bye for now friends\n");
	return;
}
