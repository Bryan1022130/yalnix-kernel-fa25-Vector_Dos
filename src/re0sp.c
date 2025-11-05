//Header files from yalnix_framework && libc library
#include <sys/types.h> //For u_long
#include <load_info.h> //The struct for load_info
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL
#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC

#define TRUE 1
#define FALSE 0

void SetupRegion0(pte_t *kernel_page_table){

	//Clear out the malloc space
	memset(kernel_page_table, 0, (MAX_PT_LEN* (sizeof(pte_t))));
	
	TracePrintf(1, "\n\n");
        TracePrintf(1,"------------------------------------------------- Region 0 Setup ----------------------------------------------------\n");

	// -------------------------------------- TEXT --------------------------------------------------
        unsigned long int text_start = _first_kernel_text_page;
        unsigned long int text_end = _first_kernel_data_page;
        TracePrintf(1," this is the value of text_start --> %lu and this is text_end --> %lu \n", text_start, text_end);

        for(unsigned long int text = text_start; text < text_end; text++){
                //Text section should be only have READ && EXEC permissions
                kernel_page_table[text].prot = PROT_READ | PROT_EXEC;
                kernel_page_table[text].valid = TRUE;
                kernel_page_table[text].pfn = text;
        }

	// -------------------------------------- DATA --------------------------------------------------
        unsigned long int heapdata_start = _first_kernel_data_page;
        unsigned long int heapdata_end = _orig_kernel_brk_page;
        TracePrintf(1," this is the value of heapdata_start --> %lu and this is heapdata_end --> %lu \n", heapdata_start, heapdata_end);

        for(unsigned long int data_heap = heapdata_start; data_heap < heapdata_end; data_heap++){
                //Heap and Data section both have READ and WRITE conditions
                kernel_page_table[data_heap].prot = PROT_WRITE | PROT_READ;
                kernel_page_table[data_heap].valid = TRUE;
                kernel_page_table[data_heap].pfn = data_heap;
        }

	// -------------------------------------- STACK --------------------------------------------------
        unsigned long int stack_start = KERNEL_STACK_BASE >> PAGESHIFT;
        unsigned long int stack_end = KERNEL_STACK_LIMIT >> PAGESHIFT;
        TracePrintf(1," this is the value of stack_start --> %lu and this is stackend --> %lu \n", stack_start, stack_end);

        for(unsigned long int stack_loop = stack_start; stack_loop < stack_end; stack_loop++){
                kernel_page_table[stack_loop].prot = PROT_READ | PROT_WRITE;
                kernel_page_table[stack_loop].valid = TRUE;
                kernel_page_table[stack_loop].pfn = stack_loop;
        }

        WriteRegister(REG_PTBR0,(unsigned int)kernel_page_table);
        WriteRegister(REG_PTLR0, (unsigned int)MAX_PT_LEN);

        TracePrintf(1,"############################################## Region 0 Setup Done ###################################################\n\n\n");
}
