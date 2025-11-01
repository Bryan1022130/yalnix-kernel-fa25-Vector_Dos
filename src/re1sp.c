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

//Macros for setting up region 1 page table 
#define VALID 1
#define INVALID 0

//Set up the region 1 user_page_table as invaid
void SetupRegion1(pte_t *user_page_table){
	//Clear out the malloc region 
	memset(user_page_table, 0, (MAX_PT_LEN * sizeof(pte_t)));

	//Set all of the region 1 space as invalid
	for(int x = 0; x < MAX_PT_LEN; x++){
		user_page_table[x].prot = 0;
                user_page_table[x].pfn = 0;
		user_page_table[x].valid = INVALID;
	}

	TracePrintf(0, "We are going to write out the base of the region 1 page table here --> %p\n", user_page_table);
	WriteRegister(REG_PTBR1, (unsigned int)user_page_table);
	WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
}
