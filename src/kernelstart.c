//Header files from yalnix_framework
#include <ykernel.h>
#include <hardware.h>
#include <ctype.h>
#include <load_info.h>
#include <yalnix.h>
#include <ylib.h>
#include <yuser.h>

//Macros
#define TRUE 1
#define FALSE 0

/* ================================>>
 * Global Variables
 * <<================================
 */

//Virtual Memory Check
short int vm_enabled = FALSE;

//Process Block
PCB *current_proccess; 

//Brk Location
void *current_kernel_brk;

//Frames available in physical memory
long int frame_count;

/* Initializing Virtual Memory
 * char *cmd_args: Vector of strings, holding a pointer to each argc in boot command line {Terminated by NULL poointer}
 * unsigned int pmem_size: Size of the physical memory of the machine {Given in byte}
 * UserContext *uctxt: pointer to an initial UserContext structure
 */ 

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt){
	//Calculate the number of page frames and store into our global variable 
	frame_count = pmem_size / PAGESIZE;

	//Page Table allocation, array of page table entries
	pte_t page_table_array[MAX_PT_ENTRY];
	
	/* <<<---------------------------------------
	 * Set up the initial Region 0 {KERNEL SPACE}
	 * --> Heap	-
	 * --> Data	-
	 *  -->Text	-
	 * --------------------------------------->>>
	 */

	/*Loop through the text section and set up the pte
	 * #include <yalnix.h>
	 * _first_kernel_text_page - > low page of kernel text
	 */

	for(long int text = _first_kernel_text_page; text < _first_kernel_data_page; text++){
		page_table_array[text].
	}


	/*Loop through heap and data sections and setup pte
	 * _first_kernel_data_page -> low page of heap/data section
	 * _orig_kernel_brk_page -> first unused page after kernel heap
	 */

	for(long int i = dataheap ; i < data_end; i++){
		page_table_array[i].
	}

	//Write the page table table base and limit registers
	WriteRegister(REG_PTBR0, (unsigned int) );
	WriteRegister(REG_PTLR0, (unsigned int) );


	WriteRegister(REG_PTBR1, (unsigned int) );
	WriteRegister(REG_PTLR1, (unsigned int) ) ;

	//The functions are to write and to read into special purpose registers
	WriteRegister(REG_PTBR0);
	ReadRegister(REG_PTLR0, );

	
	//Initialize Virtual Memory
	WriteRegister(REG_VM_ENABLE, TRUE);
	//Set the global variable as true 
	vm_enabled = TRUE;



	return;
}





void DoIdle(void) { 
	while(1) {
		TracePrintf(1,"DoIdle\n");
		Pause();
	}
}
