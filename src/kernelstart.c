//Header files from yalnix_framework
#include <ykernel.h>
#include <hardware.h>
#include <ctype.h>
#include <load_info.h>
#include <yalnix.h>
#include <ylib.h>
#include <yuser.h>
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC
#include "Queue.h"

//Macros
#define TRUE 1
#define FALSE 0

/*
 * Run this file
 * ======>  ./yalnix -W {Recommended}
 *\

 * ================================>>
 * Global Variables
 * <<================================
 */

//Virtual Memory Check
short int vm_enabled = FALSE;

//Process Block
PCB *current_process; 

//Brk Location
void *current_kernel_brk;

//Frames available in physical memory
unsigned long int frame_count;

//Virtual Memory look up logic
// Something like -----> (vpn >= vp1) ? vpn - vp1 : vpn - vp0;
unsigned long int vp0 = VMEM_0_BASE >> PAGESHIFT;
unsigned long int vp1 = VMEM_1_BASE >> PAGESHIFT;

//Page Table allocation -> an array of page table entries
//Gets Zeroed out since it is static and unintialized
static pte_t kernel_page_table[MAX_PT_LEN];
static pte_t user_page_table[MAX_PT_LEN];

//Terminal Array
//Functions for terminal {TTY_TRANSMIT && TTY_RECEIVE}
static unsigned int terminal_array[NUM_TERMINALS];

//Global array for the Interrupt Vector Table
HandleTrapCall Interrupt_Vector_Table[TRAP_VECTOR_SIZE];

/* Initializing Virtual Memory
 * char *cmd_args: Vector of strings, holding a pointer to each argc in boot command line {Terminated by NULL poointer}
 * unsigned int pmem_size: Size of the physical memory of the machine {Given in byte}
 * UserContext *uctxt: pointer to an initial UserContext structure
 */ 

void create_free_frames(void){
	//Set all pages as unused for now 
	unsigned short int bitmap[MAX_PT_LEN] = {0};

	//Maybe it can also be set in a unsigned long int???
	unsigned long int bitmap = 0;
	
	return; 
}

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt){

	/* <<<---------------------------------------------------------
	 * Boot up the free frame data structure && definee global vars
	 * ---------------------------------------------------------->>>
	 */

	//Initialize the data struct to track the free frames
	create_free_frames();

	//Calculate the number of page frames and store into our global variable 
	frame_count = pmem_size / PAGESIZE;

	/* <<<---------------------------------------
	 * Set up the initial Region 0 {KERNEL SPACE}
	 * --> Heap	-
	 * --> Data	-
	 *  -->Text	-
	 * --------------------------------------->>>
	 */

	/*Loop through the text section and set up the ptes
	 * #include <yalnix.h>
	 * _first_kernel_text_page - > low page of kernel text
	 */

	unsigned long int text_start = DOWN_TO_PAGE(_first_kernel_text_page); //Alias: Start of Text Segment
									     
	WriteRegister(REG_PTBR0,(unsigned int)kernel_page_table); //Write the address of the start of text for pte_t

	unsigned long int text_end = DOWN_TO_PAGE(_first_kernel_data_page);
	
	for(long int text = text_start; text < text_end; text++){
		//Text section should be only have READ && EXEC permissions
		kernel_page_table[text].prot = PROT_READ | PROT_EXEC;
		kernel_page_table[text].valid = TRUE;
		kernel_page_table[text].pfn = text;
	}

	/*Loop through heap and data sections and setup pte
	 * _first_kernel_data_page -> low page of heap/data section
	 * _orig_kernel_brk_page -> first unused page after kernel heap
	 */

	//Since the previous loop stopped right before the start of heap/data section
	//We have to set this pfn manually here or else it would be mapped to 2 pte
	//THIS COULD BE WRONG COME BACK AND CHECK THIS IF IT IS CORRECT
	
	unsigned long int heapdata_start = DOWN_TO_PAGE(_first_kernel_data_page); 
	unsigned long int heapdata_end = DOWN_TO_PAGE(_orig_kernel_brk_page);

	for(long int data_heap = heapdata_start; data_heap < heapdata_end; data_heap++){
		//Heap and Data section both have READ and WRITE conditions
		kernel_page_table[data_heap].prot = PROT_WRITE | PROT_READ; 
		kernel_page_table[data_heap].valid = TRUE;
		kernel_page_table[data_heap].pfn = data_heap;
	}

	unsigned long int stack_start = DOWN_TO_PAGE(KERNEL_STACK_BASE);
	unsigned long int stack_end = DOWN_TO_PAGE(KERNEL_STACK_LIMIT);

	for(long int stack_loop = stack_start; stack_loop < stack_end; stack_loop++){
		kernel_page_table[stack_loop].prot = PROT_READ | PROT_WRITE;
		kernel_page_table[stack_loop].valid = TRUE;
		kernel_page_table[stack_loop].pfn = stack_loop; 

	}

	//Write the page table table limit register for Region 0
	//We can pass in heapdata_end because REG_PTLR0 needs number of entries in the page table for region 0
	//MAX_PT_ENTRY {maybe} 
	WriteRegister(REG_PTLR0, (unsigned int)MAX_PT_LEN);

	/* <<<------------------------------
	 * Call SetKernelBrk()
	 * ------------------------------>>>
	 */
	
	int kbrk_return = SetKernelBrk(current_kernel_brk);

	if(kbrk_return != 0){
		TracePrintf(0, "There was an error in SetKernelBrk");
		return;
	}

	/* <<<------------------------------
	 * Set up the Interrupt Vector Table
	 * ------------------------------>>>
	 */

	setup_trap_handler(Interrupt_Vector_Table);

	/* <<<--------------------------
	 * Initialize Virtual Memory
	 * -------------------------->>>
	 */
		
	//Write to special register that Virtual Memory is enabled 
	WriteRegister(REG_VM_ENABLE, TRUE);

	//Set the global variable as true 
	vm_enabled = TRUE;


	/* <<<-------------------------------------
	 * Create Process
	 * ------------------------------------->>>
	 */



	//Write the page table table base and limit registers for Region 1
	WriteRegister(REG_PTBR1, (unsigned int) );
	WriteRegister(REG_PTLR1, (unsigned int) ) ;

	return;
}


void DoIdle(void) { 
	while(1) {
		TracePrintf(1,"DoIdle\n");
		Pause();
	}
}
