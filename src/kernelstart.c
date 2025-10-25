//Header files from yalnix_framework
#include <ykernel.h>
#include <hardware.h> // For macros regarding kernel space
#include <ctype.h>
#include <load_info.h>
#include <yalnix.h>
#include <ylib.h>
#include <yuser.h>
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC
#include "Queue.h"
#include "trap.h"
#include "memory.h"
//Macros
#define TRUE 1
#define FALSE 0

/* ==================================
 * Run this file for checkpoint 1
 * ======>  ./yalnix -W {Recommended}
 * ==================================
 *\

 * ================================>>
 * Global Variables
 * <<================================
 */

//Virtual Memory Check
short int vm_enabled = FALSE;

//Process Block
PCB *current_process; 
PCB *idle_process;
PCB *process_read_head;

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


void create_free_frames(void){
	//Set all pages as unused for now 
	unsigned short int bitmap[MAX_PT_LEN] = {0};

	//Maybe it can also be set in a unsigned long int???
	unsigned long int bitmap = 0;
	
	return; 
}

/* =======================================
 * Process Logic Functions
 * =======================================
 */

void init_proc_create(void){
	//Need to work on this ----------------------<<<<<<>>>>>>>
	//MAKING THE  PCB ALLOC FUNCTIONS
	//Get a process from our PCB free list
	idle_process = pcb_alloc();

	if(idle_process == NULL){
		PrintTracef(0, "There was an error when trying with pcb_alloc, NULL returned!");
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
	idle_process->curr_uc.sp = (void*)VMEM_1_LIMIT;;

	//Set as running
	idle_process->currState = Running;
	
	//Track Kernel Stack Frames
	

	//Set global variable for current process as the idle process
	current_process = idle_process;

	memcpy(KernelUC, &idle_process->curr_uc, sizeof(UserContext));

}

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

/* ========================================================
 * Initializing Virtual Memory
 * char *cmd_args: Vector of strings, holding a pointer to each argc in boot command line {Terminated by NULL poointer}
 * unsigned int pmem_size: Size of the physical memory of the machine {Given in bytes}
 * UserContext *uctxt: pointer to an initial UserContext structure
 * =========================================================
 */ 

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt){

	/* <<<---------------------------------------------------------
	 * Boot up the free frame data structure && definee global vars
	 * ---------------------------------------------------------->>>
	 */

	//Initialize the data struct to track the free frames
	frames_init(pmem_size);

	//Calculate the number of page frames and store into our global variable 
	frame_count = pmem_size / PAGESIZE;
	
	//Set up global variable for UserContext
	KernelUC = uctxt;

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

	//Write the address of the start of text for pte_t
	WriteRegister(REG_PTBR0,(unsigned int)kernel_page_table);
	
	//Set the Global variable
	kernel_region_pt = (void *)kernel_page_table;
	
	unsigned long int text_start = DOWN_TO_PAGE((unsigned long)_first_kernel_text_page);
	unsigned long int text_end = DOWN_TO_PAGE((unsigned long)_first_kernel_data_page);
	
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

	unsigned long int heapdata_start = DOWN_TO_PAGE((unsigned long)_first_kernel_data_page); 
	unsigned long int heapdata_end = DOWN_TO_PAGE((unsigned long)_orig_kernel_brk_page);

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

	unsigned long int stack_start = DOWN_TO_PAGE(KERNEL_STACK_BASE);
	unsigned long int stack_end = DOWN_TO_PAGE(KERNEL_STACK_LIMIT);

	for(unsigned long int stack_loop = stack_start; stack_loop < stack_end; stack_loop++){
		kernel_page_table[stack_loop].prot = PROT_READ | PROT_WRITE;
		kernel_page_table[stack_loop].valid = TRUE;
		kernel_page_table[stack_loop].pfn = stack_loop; 
	}

	//Write the page table table limit register for Region 0
	//MAX_PT_LEN because REG_PTLR0 needs number of entries in the page table for region 0
	WriteRegister(REG_PTLR0, (unsigned int)MAX_PT_LEN);
	
	//Set global variable for stack limit
	kernel_stack_limit = VMEM_0_LIMIT;
	
	/* <<<------------------------------
	 * Set up the Interrupt Vector Table
	 * ------------------------------>>>
	 */

	setup_trap_handler(Interrupt_Vector_Table);

	/* <<<-----------------------------------
	 * Set Up Region 1 for the Idle Process
	 * ----------------------------------->>>
	 */

	//Write the base of the region 1 memory
	WriteRegister(REG_PTBR1, (unsigned int)user_page_table);

	//Already allocated regionn 1 Page table as global
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
	current_kernel_brk = (void *)_orig_kernel_brk_page;

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
	init_proc_create();

	TracePrintf(0, "I am leaving KernelStart");
	return;
}
