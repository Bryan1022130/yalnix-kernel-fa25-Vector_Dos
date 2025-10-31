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

#define FALSE 0
#define TRUE 1

extern pte_t kernel_page_table[MAX_PT_LEN];

/* ===========================================
 * Function for setting up the init function
 * ===========================================
 */

PCB *createInit(void){
        TracePrintf(0, "We are creating the init process {This should be process 2}\n");

        PCB *init_proc = pcb_alloc();

        if(init_proc == NULL){
                TracePrintf(0, "There was an error when getting a process for Init Process\n");
                return NULL;
        }

        //Since the parent process will have a pid of 0; Set the ppid to zero
        init_proc->ppid = 0;

        // Allocate a physical frame for the page table
        int pt_pfn = frame_alloc(init_proc->pid);

        //WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
         if (pt_pfn == ERROR) {
             TracePrintf(0, "idle_proc_create(): ERROR allocating PT frame\n");
             pcb_free(init_proc->pid);
             return NULL;
         }

         TracePrintf(1, "InitProc: PT frame pfn=%d\n", pt_pfn);

         // Map it temporarily into kernel space to initialize it
         // Find a free virtual page in kernel space to map this frame

         int temp_vpn = -1;
         //Look downward for free space to not reused pages by accident
         //Once again make this with a data structure
         for (int i = (KERNEL_STACK_BASE >> PAGESHIFT) - 1; i > _orig_kernel_brk_page; i--) {
                 if (kernel_page_table[i].valid == FALSE) {
                         temp_vpn = i;
                         break;
                 }
         }

        if (temp_vpn < 0) {
                TracePrintf(0, "idle_proc_create(): ERROR no free kernel vpn for PT mapping\n");
                frame_free(pt_pfn);
                pcb_free(init_proc->pid);
                return NULL;
        }

        //Map the pfn into the kernel_page_table so that it can be accessed by MMU
        kernel_page_table[temp_vpn].pfn = pt_pfn;
        kernel_page_table[temp_vpn].prot = PROT_READ | PROT_WRITE;
        kernel_page_table[temp_vpn].valid = TRUE;

        TracePrintf(0, "FLushing Region 1 memory space so that it knows its updated\n");
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

        // Get pointer to the page table; we are getting the virtual address with temp_vpn << PAGESHIFT
        //This serves as the blueprint to talk to physical memory
        pte_t *init_pt = (pte_t *)(temp_vpn << PAGESHIFT);

        TracePrintf(0, "About to call memset on v_addr %p (vpn %d)\n", init_pt, temp_vpn);

        memset(init_pt, 0, MAX_PT_LEN * sizeof(pte_t));

        init_proc->AddressSpace = (void *)(temp_vpn << PAGESHIFT);

        return init_proc;
}

void init(void){
	while(1){
		TracePrintf(0, "This called when there is no arguments passed to ./yalnix\n");
		Pause();
	}

}
