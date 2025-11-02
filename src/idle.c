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
#include "process.h"

//Macros 
#define TRUE 1
#define FALSE 0

//Extern
extern UserContext *KernelUC;
extern PCB *current_process;

/* ===================================================================================================================
 * Idle Function that runs in Kernel Space
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
 * Idle Process that runs when there is not other process being runned 
 * ===================================================================================================================
 */
void idle_proc_create(unsigned char *track, int track_size, pte_t *user_page_table, UserContext *uctxt){
        TracePrintf(0, "Start of the idle_proc_create function <|> \n");

        //Malloc space for PCB idle struct
        PCB *idle_process = (PCB *)malloc(sizeof(PCB));

        //Point to our user_page_table and clear the table
        pte_t *idle_pt = user_page_table;
        memset(idle_pt, 0, sizeof(pte_t) * MAX_PT_LEN);

        //Get a pid from the help of hardware
        int pid_find = helper_new_pid(user_page_table);
        idle_process->pid = pid_find;

        //Allocate a physical page for the process
        int pfn = find_frame(track, track_size);
        if(pfn == ERROR){
                TracePrintf(0, "No frames were found!\n");
                return;
        }

        //We are storing it at the top of the user stack region
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

        //Copy in idle PCB properties into the KernelUC
        memcpy(KernelUC, &idle_process->curr_uc, sizeof(UserContext));

        /* =======================================
         * Write region 1 table to Hardware
         * =======================================
         */

        WriteRegister(REG_PTBR1, (unsigned int)user_page_table);
        WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

        //write the current UserContext back into uctxt
        memcpy(uctxt, &idle_process->curr_uc, sizeof(UserContext));

        //Set global variable for current process as the idle process
        current_process = idle_process;

        TracePrintf(0, "===+++++++++++++++++++++++++ IDLE PROCESS DEBUG +++++++++++++++++++++++++++++++++++++++++++====\n");
        TracePrintf(0, " This is the num of the array for the kernel_page_table --> %d\n", MAX_PT_LEN);
        TracePrintf(0, "This is idle pc -- > %p and this is sp --> %p\n", idle_process->curr_uc.pc, idle_process->curr_uc.sp);
        TracePrintf(0, "idle_process ptr =------------->>> : %p\n", idle_process);
        TracePrintf(0, "idle_process->pid: %d\n", idle_process->pid);
        TracePrintf(0, "This is the value of the VMEM_1_LIMIT ==> %p and this is the VMEM_1_BASE ==> %p\n", VMEM_1_LIMIT, VMEM_1_BASE);
        TracePrintf(0, "current_process ptr: %p\n", current_process);
        TracePrintf(0, "(This is our region 1 Addressspace= %p\n", idle_process->AddressSpace);
        TracePrintf(0, "===============================================================================================\n");
        TracePrintf(0, "End of the idle_proc_create function <|> \n\n");
}


