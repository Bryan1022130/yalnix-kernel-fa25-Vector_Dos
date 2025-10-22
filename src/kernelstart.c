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

	for(long int i = 0; i < MAX_PT_ENTRY; i++){
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
