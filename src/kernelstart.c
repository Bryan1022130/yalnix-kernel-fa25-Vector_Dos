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
//User table needs to be private to each process

//Terminal Array
//Functions for terminal {TTY_TRANSMIT && TTY_RECEIVE}
static unsigned int terminal_array[NUM_TERMINALS];

//Global array for the Interrupt Vector Table
HandleTrapCall Interrupt_Vector_Table[TRAP_VECTOR_SIZE];

/* ===================================================================================================================
 * Idle Function that runs in Kernel Space
 * Simple idle loop that runs when no processes are ready.
 * Continuously calls Pause() to yield CPU.
 * ===================================================================================================================
 */

void DoIdle(void) { 
	int count = 0;
	while(1) {
		TracePrintf(1,"Idle loop running (%d)\n", count++);
		Pause();
	}
}

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
    for (int i = 0; i < MAX_PROCS; i++) {
        process_table[i].currState = FREE;
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
	TracePrintf(1, "Allocating new PCB...\n");
	for (int pid = 0; pid < MAX_PROCS; pid++) {
		if (process_table[pid].currState == FREE) {
	
			//Clear out the data in case there is left over data
			memset(&process_table[pid], 0, sizeof(PCB));
			
			TracePrintf(0, "Creating the Kernel Stack for the Process this many --> %d\n", KERNEL_STACK_PAGES);

			for(int i = 0; i < KERNEL_STACK_PAGES; i++){

				int pfn = frame_alloc(pid);

				if(pfn == ERROR){

					TracePrintf(0, "We ran out of frames to give!\n");

					//Unmap any previous frames that we allocated

					for(int f = 0; f < i; f++){
						frame_free(process_table[pid].kernel_stack_frames[f]);
					}

					return NULL;
				}

				process_table[pid].kernel_stack_frames[i] = pfn;

			}

			process_table[pid].currState = READY; 
			process_table[pid].pid = pid;
			
			TracePrintf(1, "Allocated PCB with PID %d \n", pid);
			return &process_table[pid];
		}
	}

	TracePrintf(0, "ERROR: No free PCBs available.\n");
	return NULL;
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
	
	//Invalid pid check
	if(pid < 0 || pid >= MAX_PROCS){
        	TracePrintf(0, "Invalid PID passed to pcb_free().\n");
        	return ERROR;
    	}
	
	//if the process is already free
    	if(process_table[pid].currState == FREE){
        	TracePrintf(1, "Process %d already FREE.\n");
        	return ERROR;
    	}

    	//Index into buffer to get the PCB at index pid 
   	PCB *proc = &process_table[pid];

    	if (proc->AddressSpace != NULL) {

        // Extract PFN from the physical address stored in AddressSpace
        long int pt_pfn = (uintptr_t)proc->AddressSpace >> PAGESHIFT;

	TracePrintf(1, "Freeing Region 1 for PID %d (PT PFN=%d)\n", pid, pt_pfn);
        
        // Find a free kernel virtual page to map temp && loop by page number and not byte address
        int temp_vpn = -1;
        for (int i = _orig_kernel_brk_page; i < (KERNEL_STACK_BASE >> PAGESHIFT); i++) {
            if (kernel_page_table[i].valid == FALSE) {
                temp_vpn = i;
                break;
            }
        }
       
	//If no page found then it will remain -1
        if (temp_vpn < 0) {
            TracePrintf(0, "ERROR: No free kernel page available for temporary mapping.\n");
            return ERROR;
        }
        
        //Since the Kernel can not read from physical memory, map into virtual
        kernel_page_table[temp_vpn].pfn = pt_pfn; 
        kernel_page_table[temp_vpn].prot = PROT_READ | PROT_WRITE;
        kernel_page_table[temp_vpn].valid = 1;
	
	//Flush so that the new page is valid
        WriteRegister(REG_TLB_FLUSH, (unsigned int)(temp_vpn << PAGESHIFT)); 

        // Now access the page table
        pte_t *pt_r1 = (pte_t *)(temp_vpn << PAGESHIFT);
        
        // Free all frames mapped in the process's Region 1
        int region1_pages = MAX_PT_LEN - 1; //BASED ON THE DIAGRAM
	
	TracePrintf(1, "This is the value of the frames mapped in this process current region 1 == > %d\n", region1_pages);
	
	//If a page is mapped, unmap it
	for (int vpn = 0; vpn < region1_pages; vpn++) {
		if (pt_r1[vpn].valid == TRUE) {
			frame_free(pt_r1[vpn].pfn);
		}
	}

	//Set the kernel page that we used as invalid again
        kernel_page_table[temp_vpn].valid = FALSE;
        kernel_page_table[temp_vpn].prot = 0;
	kernel_page_table[temp_vpn].pfn = 0;
	
	//WriteRegister need the a byte address, hence why << PAGESHIFT
	WriteRegister(REG_TLB_FLUSH, (unsigned int)(temp_vpn << PAGESHIFT));
        
        // Free the physical frame that held the page table itself
	// This came from the AddressSpace field in our PCB
        frame_free(pt_pfn);
    };

	TracePrintf(1, "This is the value of the KERNEL_STACK_PAGES --> %d\n", KERNEL_STACK_PAGES);
	
	//Free physical frames that are associated with the procs kernel stack
	//The kernel_stack_frame holds pfns for frames in main memory
	for (int i = 0; i < KERNEL_STACK_PAGES; i++) {
		if (proc->kernel_stack_frames[i] > 0) {
			frame_free(proc->kernel_stack_frames[i]);
		}
	}

	// Clear the PCB
	memset(proc, 0, sizeof(PCB));
    	proc->currState = FREE;
    
    	TracePrintf(1, "Freed PCB for PID %d.\n");
    	return 0;
}

//==================================================================================================================
void init_proc_create(void){

	TracePrintf(1, "init_proc_create(): begin\n");

	//Get a process from our PCB free list
	idle_process = pcb_alloc();

	if(idle_process == NULL){
		TracePrintf(0, "init_proc_create(): ERROR pcb_alloc() returned NULL\n");
		return;
	}

  	/* =======================
    	 * Allocate a new page table for idle process
     	 * =======================
     	 */

    	// Allocate physical frame for the page table
   	 int pt_pfn = frame_alloc(idle_process->pid);

   	 if (pt_pfn == ERROR) {
   	     TracePrintf(0, "init_proc_create(): ERROR allocating PT frame\n");
	     return;
   	 }

	 TracePrintf(1, "init_proc_create(): PT frame pfn=%d\n", pt_pfn);

    	 // Map it temporarily into kernel space to initialize it
  	 // Find a free virtual page in kernel space to map this frame
	 int temp_vpn = -1;
	 for (int i = _orig_kernel_brk_page; i < (KERNEL_STACK_BASE >> PAGESHIFT); i++) {
		 if (kernel_page_table[i].valid == FALSE) {
			 temp_vpn = i;
			 break;
        	}
   	 }
	 TracePrintf(0, "HELLO\n");

    	if (temp_vpn < 0) {
		TracePrintf(0, "init_proc_create(): ERROR no free kernel vpn for PT mapping\n");
		frame_free(pt_pfn);
		return;
    	}
  	
	//Map it into the virtual kernel table for now
	//This is for the MMU
   	kernel_page_table[temp_vpn].pfn = pt_pfn;
    	kernel_page_table[temp_vpn].prot = PROT_READ | PROT_WRITE;
    	kernel_page_table[temp_vpn].valid = TRUE;
	TracePrintf(0, "FLushing Region 1 memory space so that it knows its updated\n");
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
	TracePrintf(0, "FLUSH IS DONE.\n");

   	// Get pointer to the page table; we are getting the virtual address with temp_vpn << PAGESHIFT
   	pte_t *idle_pt = (pte_t *)(temp_vpn << PAGESHIFT);
	TracePrintf(0, "About to call memset on v_addr %p (vpn %d)\n", idle_pt, temp_vpn);

    	// Clear out the page table to start fresh
    //	memset(idle_pt, 0, PAGESIZE);
   //	TracePrintf(0, "MEMSET SUCCEEDED.\n");

   	// Allocate stack for idle process
	TracePrintf(0, "About to alloc stack frame...\n");
   	int idle_stack_pfn = frame_alloc(idle_process->pid);

	TracePrintf(0, "Am i being called?\n");
	if (idle_stack_pfn == ERROR) {
		TracePrintf(0, "init_proc_create(): ERROR allocating stack frame\n");
		frame_free(pt_pfn);
		return;
	}

	TracePrintf(1, "init_proc_create(): stack frame pfn=%d\n", idle_stack_pfn);
    	
	//Map into kernel 
    	unsigned long stack_page_index = MAX_PT_LEN - 1;
    	idle_pt[stack_page_index].valid = TRUE;
    	idle_pt[stack_page_index].prot = PROT_READ | PROT_WRITE;
   	idle_pt[stack_page_index].pfn = idle_stack_pfn;

	/* =======================
	 * Pid Logic
	 * =======================
	 */
	
	//Get a pid for the process
	idle_process->pid = helper_new_pid(idle_pt);
	TracePrintf(0, "init_proc_create(): assigned pid=%d\n", idle_process->pid);

	//To indicate that its the kernel process itself
	idle_process->ppid = 0;
	
	/* =======================================
	 * Store AddressSpace for region 1 table
	 * =======================================
	 */
	WriteRegister(REG_PTBR1, (unsigned int)(temp_vpn << PAGESHIFT));
	WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
	//Flush for region 1 since we just wrote its start and limit
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

	idle_process->AddressSpace = (void *)(uintptr_t)(temp_vpn << PAGESHIFT);
	
	//Set sp to the top of the user stack that we set up
	idle_process->curr_uc.pc = (void*)DoIdle;
	idle_process->curr_uc.sp = (void*)(VMEM_1_LIMIT - 1);
	TracePrintf(0, "This is the value of the idle pc -- > %d and this is the value of the sp --> %d\n", idle_process->curr_uc.pc, idle_process->curr_uc.sp);

	/* ======================================================
	 * Copy in UserContext from KernelStart into the idle PCB
	 * ======================================================
	 */

	memcpy(KernelUC, &idle_process->curr_uc, sizeof(UserContext));
		
	//Set as running
	idle_process->currState = READY;

	//Set global variable for current process as the idle process
	current_process = idle_process;

	TracePrintf(0, "===+++++++++++++++++++++++++ IDLE PROCESS DEBUG +++++++++++++++++++++++++++++++++++++++++++====\n");
	TracePrintf(0, "idle_process ptr: %p\n", idle_process);
	TracePrintf(0, "idle_process->pid: %d\n", idle_process->pid);
	TracePrintf(0, "idle_process->AddressSpace: %p\n", idle_process->AddressSpace);
	TracePrintf(0, "pt_pfn: %d (0x%x)\n", pt_pfn, pt_pfn);
	TracePrintf(0, "physical addr: 0x%lx\n", (unsigned long)(pt_pfn << PAGESHIFT));
	TracePrintf(0, "current_process ptr: %p\n", current_process);
	TracePrintf(0, "==========================\n");
	
	TracePrintf(0, "init_proc_create(): done (pid=%d, aspace=%p)\n", idle_process->pid, idle_process->AddressSpace);

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
		TracePrintf(2, "THIS IS CALLED WHEN VIRTUAL MEMORY IS NOT ENABLED\n");	

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
		TracePrintf(2, "VIRTUAL MEMORY HAS BEEN ENABLED \n");

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


		TracePrintf(2, "[SetKernelBrk-VM] Comparing new_brk: %p against heap_start: %p\n",
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
							
		// If grow_end > grow_start → we’re expanding the heap
		// If grow_end < grow_start → we’re shrinking

		// Step 2: Growing the heap (allocate frames)
		if (grow_end > grow_start) {
		        TracePrintf(2, "[SetKernelBrk] Growing kernel heap\n");

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
		                        TracePrintf(0, "[SetKernelBrk] Out of frames while growing!\n");
		                        return ERROR;
		                }

		                kernel_page_table[vpn].pfn   = pfn;
		                kernel_page_table[vpn].prot  = PROT_READ | PROT_WRITE;
		                kernel_page_table[vpn].valid = TRUE;

		                mapped_until = vaddr + PAGESIZE;
		        }

		        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
		}
		else if (grow_end < grow_start) {
		        TracePrintf(2, "[SetKernelBrk] Shrinking kernel heap...\n");
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

	//Calculate the number of page frames and store into our global variable 
	frame_count = (pmem_size / PAGESIZE);

	TracePrintf(1,"========> This is the start of the KernelStart function <=====================\n");
	TracePrintf(2, "<<<<<<<<<<<<<<<< this is the size pmem_size => %lX and this is the stack frames ==> %d >>>>>>>>>>>>>>>>>>>>>>>>\n", pmem_size, frame_count);

	/* <<<---------------------------------------------------------
	 * Boot up the free frame data structure && define global vars
	 * ---------------------------------------------------------->>>
	 */

	//Initialize the data struct to track the free frames
	// Builds the frame table and marks kernel-reserved frames as used.
	frames_init(pmem_size);

	//Set up global variable for the current UserContext
	KernelUC = uctxt;
	// KernelStart will copy or modify this to return into user mode later.

	/* <<<------------------------------
	 * Set up the Interrupt Vector Table
	 * ------------------------------>>>
	 */

	// Each trap/interrupt is linked to its handler so hardware can dispatch correctly.
	// This must be complete before enabling VM or the kernel will fault.
	TracePrintf(1,"+++++ We are setting up the IVT and im going to call the function\n");
	setup_trap_handler(Interrupt_Vector_Table);
	TracePrintf(1,"+++++ We have left the functions and going to set up region 0\n");

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


	// Build Region 0 mappings for kernel text, data, heap, and stack.  
	// This must be complete before enabling VM or the kernel will fault.	
	TracePrintf(1,"-------------A------------------------------------ Region 0 Set up ----------------------------------------------------\n");

	//Set the Global variable
	kernel_region_pt = (void *)kernel_page_table;
	TracePrintf(2,"This is where my array is located --> %p\n", kernel_region_pt);
	
	unsigned long int text_start = _first_kernel_text_page;
	TracePrintf(2, "This is the value of the text_start --> %lx ++++++++++++++= \n", text_start);
	unsigned long int text_end = _first_kernel_data_page;
	TracePrintf(2, "This is the value of the text_end ---> %lx++++++++++++++\n", text_end);
	
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
	TracePrintf(2, "This is the value of the heapdata_start---> %lx++++++++++++++++\n", heapdata_start);
	unsigned long int heapdata_end = _orig_kernel_brk_page;
	TracePrintf(2, "This is the value of the heapdata_end ---> %lx ++++++++++++++++++++\n", heapdata_end);

	for(unsigned long int data_heap = heapdata_start; data_heap < heapdata_end; data_heap++){
		//Heap and Data section both have READ and WRITE conditions
		kernel_page_table[data_heap].prot = PROT_WRITE | PROT_READ; 
		kernel_page_table[data_heap].valid = TRUE;
		kernel_page_table[data_heap].pfn = data_heap;
	}

	/* ==============================
	 * Red Zone {Unmapped Pages}
	 * ==============================
	 */

	unsigned long int stack_start = KERNEL_STACK_BASE >> PAGESHIFT;
	unsigned long int stack_end = KERNEL_STACK_LIMIT >> PAGESHIFT;
	
	TracePrintf(2,"This is in the stack loop logic\n");
	TracePrintf(2," this is the value of stack_start --> %lx and this is stackend --> %lx \n", stack_start, stack_end);

	for(unsigned long int stack_loop = stack_start; stack_loop < stack_end; stack_loop++){
		TracePrintf(2,"WE ARE in the stack loop\n");
		kernel_page_table[stack_loop].prot = PROT_READ | PROT_WRITE;
		kernel_page_table[stack_loop].valid = TRUE;
		kernel_page_table[stack_loop].pfn = stack_loop; 
	}

	TracePrintf(2,"THIS IS BEFORE WE ARE WRITING THE PAGE TABLE ADDRESS TO THE HARDWARE\n");

	//Set global variable for stack limit for region 0
	kernel_stack_limit = (void *)KERNEL_STACK_LIMIT;

	//Write the address of the start of text for pte_t
	WriteRegister(REG_PTBR0,(unsigned int)kernel_page_table);
	
	//MAX_PT_LEN because REG_PTLR0 needs number of entries in the page table for region 0
	WriteRegister(REG_PTLR0, (unsigned int)MAX_PT_LEN);

	TracePrintf(1, "Region 0 setup complete (stack, text, data, heap)\n");

	pte_t *kernel_pt = kernel_page_table;

	int kernel_pid = helper_new_pid(kernel_pt);

	TracePrintf(2, "THIS IS THE KERNEL PID ++================================>>>>>>>>>>>>>>>>>>>>>>>>>>>> %d\n", kernel_pid);

	if (kernel_pid < 0) {
		TracePrintf(0, "FATAL ERROR: Could not register kernel's initial PID!\n");
   		 Halt(); // Or appropriate error handling
	}

	/* <<<--------------------------
	 * Initialize Virtual Memory
	 * -------------------------->>>
	 */
	
	TracePrintf(1, "------------------------------------- WE ARE TURNING ON VIRTUAL MEMORY NOW :) -------------------------------------\n");
	//Write to special register that Virtual Memory is enabled 

	// All Region 0 mappings are ready; now turn on virtual memory.  
	// From this point, all addresses are translated via the page tables.
	WriteRegister(REG_VM_ENABLE, TRUE);

	/* <<<------------------------------
	 * Call SetKernelBrk()
	 * ------------------------------>>>
	 */

	//Set current brk and then call SetKernelBrk
	TracePrintf(1, "------------------------------------- WE ARE NOW GOING TO CALL KERNELBRK :) -------------------------------------\n");

	//Normalize into byte address
	// Initialize kernel heap pointer (current_kernel_brk)  
	// and test SetKernelBrk() to ensure heap pages map correctly under VM.
    	uintptr_t orig_brk_address = (uintptr_t)_orig_kernel_brk_page * PAGESIZE;
	current_kernel_brk = (void *)orig_brk_address;
	int kbrk_return = SetKernelBrk(current_kernel_brk);

	if(kbrk_return != 0){
		TracePrintf(0, "There was an error in SetKernelBrk\n");
		return;
	}

	TracePrintf(1, "We have called set KernelBrk :)\n");

	//Set the global variable as true 
	vm_enabled = TRUE;

	/* <<<-------------------------------------
	 * Create Process
	 * ------------------------------------->>>
	 *
	*/
	TracePrintf(1, "KernelStart: creating idle process\n");

	//Create idle proc
	//  Initialize the process table and create the first (idle) process.  
	// This simulates the ‘init’ process until real user loading is implemented.
	InitPcbTable();

	// Each process needs a private Region 1 page table.  
	// Allocate one physical frame to hold it.
	init_proc_create();

	TracePrintf(1, "KernelStart complete.\n");
	return;
}
