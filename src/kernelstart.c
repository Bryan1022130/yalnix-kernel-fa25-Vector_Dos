//Header files from yalnix_framework
#include <ykernel.h>
#include <hardware.h>
#include <ctype.h>
#include <load_info.h>
#include <yalnix.h>
#include <ylib.h>
#include <yuser.h>
#include <sys/mman.h>

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

	/*Loop through the text section and set up the ptes
	 * #include <yalnix.h>
	 * _first_kernel_text_page - > low page of kernel text
	 */

	unsigned long int pfn_track = DOWN_TO_PAGE(_first_kernel_text_page);
	unsigned long int text_end = DOWN_TO_PAGE(_first_kernel_data_page);
	
	for(long int text = pfn_track; text < text_end; text++){
		//Text section should be only have READ && EXEC permissions
		page_table_array[text].prot = PROT_READ | PROT_EXEC;
		page_table_array[text].valid = FALSE;
		page_table_array[text].pfn = pfn_track;

		//Increase by PAGESIZE to store the physical memory addr in pte_t.pfn
		pfn_track+= PAGESIZE;
	}

	/*Loop through heap and data sections and setup pte
	 * _first_kernel_data_page -> low page of heap/data section
	 * _orig_kernel_brk_page -> first unused page after kernel heap
	 */

	unsigned long int heapdata_start = DOWN_TO_PAGE(_first_kernel_data_page); 
	unsigned long int heapdata_end = DOWN_TO_PAGE(_orig_kernel_brk_page);

	for(long int dataheap = heapdata_start; dataheap < heapdata_end; dataheap++){
		//Heap and Data section both have READ and WRITE conditions
		page_table_array[dataheap].prot = PROT_WRITE | PROT_READ; 
		page_table_array[dataheap].valid = FALSE;
		page_table_array[dataheap].pfn = pfn_track;

		//Increase by PAGESIZE as before
		pfn_track += PAGESIZE;
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
