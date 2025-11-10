//Header files from yalnix_framework && libc library
#include <sys/types.h> //For u_long
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL
#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC	      
#include "process.h"
#include "idle.h"

//Macros 
#define TRUE 1
#define FALSE 0

//Extern
extern UserContext *KernelUC;
extern PCB *current_process;
extern PCB *idle_process;
extern pte_t *kernel_page_table;

/* ===================================================================================================================
 * Idle Function that runs in Kernel Space
 * Continuously calls Pause() to yield CPU.
 * ===================================================================================================================
 */

void DoIdle(void) {
        int count = 0;
        while(1) {
                TracePrintf(1,"Idle loop running (%d)\n", count++);
		TracePrintf(1,"Idle loop running (%d)\n", count++);
		TracePrintf(1,"Idle loop running (%d)\n", count++);
		TracePrintf(1,"Idle loop running (%d)\n", count++);
		TracePrintf(1,"Idle loop running (%d)\n", count++);
		TracePrintf(1,"Idle loop running (%d)\n", count++);
		TracePrintf(1,"Idle loop running (%d)\n", count++);
		TracePrintf(1,"Idle loop running (%d)\n", count++);
		TracePrintf(1,"Idle loop running (%d)\n", count++);
		TracePrintf(1,"Idle loop running (%d)\n", count++);
                Pause();
        }
}

/* ===================================================================================================================
 * Idle Process that runs when there is not other process being runned 
 * ===================================================================================================================
 */
int idle_proc_create(unsigned char *track, pte_t *user_page_table, UserContext *uctxt){
        TracePrintf(0, "Start of the idle_proc_create function <|> \n");

        //Create idle_process and clear the memory 
	idle_process = calloc(1, sizeof(PCB));

        //Point to our user_page_table and clear the table
        pte_t *idle_pt = user_page_table;
        memset(idle_pt, 0, sizeof(pte_t) * MAX_PT_LEN);

        //Get a pid from the help of hardware
        idle_process->pid = helper_new_pid(user_page_table);

       //Allocate a physical page for the process
       int pfn = find_frame(track);
        if(pfn == ERROR){
		free(idle_process);
                TracePrintf(0, "No frames were found!\n");
                return ERROR;
        }

        //We are storing it at the top of the user stack region
	//This should have one valid page, for idle’s user stack	
        unsigned long stack_page_index = MAX_PT_LEN - 1;
        idle_pt[stack_page_index].valid = TRUE;
        idle_pt[stack_page_index].prot = PROT_READ | PROT_WRITE;
        idle_pt[stack_page_index].pfn = pfn;

        /* =======================
         * idle_process field setup
         * =======================
         */

        idle_process->AddressSpace = user_page_table;
        idle_process->curr_uc.pc = (void*)DoIdle;
        idle_process->curr_uc.sp = (void*)(VMEM_1_LIMIT - 1);
        idle_process->currState = READY;

        /* =======================================
         * Write region 1 table to Hardware
         * =======================================
         */

	//Mark its kernel stack frames
	idle_process->kernel_stack_frames[0] = 126;
	idle_process->kernel_stack_frames[1] = 127;

        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

        WriteRegister(REG_PTBR1, (unsigned int)user_page_table);
        WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

        //write the current UserContext back into uctxt so the hardware knows
	memcpy(uctxt, &idle_process->curr_uc, sizeof(UserContext));

        //Set global variable for current process as the idle process
        current_process = idle_process;
	
	TracePrintf(0, "End of the idle_proc_create function <|> \n\n\n");

	return SUCCESS;
}


