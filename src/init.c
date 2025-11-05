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
#include "trap.h" //API for trap handling and initializing the Interrupt Vector Table
#include "memory.h" //API for Frame tracking in our program
#include "process.h" //API for process block control
#include "Queue.h"

//Macros
#define FALSE 0
#define TRUE 1

//Extern 
extern pte_t *kernel_page_table;
extern PCB *current_process;
extern UserContext *KernelUC;
extern unsigned long int frame_count;
extern Queue *readyQueue;
extern PCB *idle_process;

PCB *create_init_proc(unsigned char *track, int track_size){
        TracePrintf(0, "Start of the init process </> \n");

        PCB *init_proc =(PCB *)malloc(sizeof(PCB));
        if(init_proc == NULL){
                TracePrintf(0, "Malloc error for init_proc\n");
		free(init_proc);
                return NULL;
        }

	memset(init_proc, 0, sizeof(PCB));
	
	//Set up its Kernel Frames
	int kf_ret = create_sframes(init_proc, track, frame_count);
	if(kf_ret == ERROR){
		TracePrintf(0, "Init Proc: Error with setting up kernel stack!\n");
		free(init_proc);
		return NULL;
	}
	
	//Malloc new regions space for 1
	pte_t *proc_space = (pte_t *)malloc(MAX_PT_LEN * sizeof(pte_t));
	if(proc_space == NULL){
		TracePrintf(0,"There was an error with malloc for region 1 space");
		free(init_proc);
		return NULL;
	}

	init_proc->pid = helper_new_pid(proc_space);
	memset(proc_space, 0, sizeof(MAX_PT_LEN * sizeof(pte_t)));
	init_proc->AddressSpace = proc_space;

	//a UserContext (from the the uctxt argument to KernelStart
	memcpy(&init_proc->curr_uc, KernelUC, sizeof(UserContext));
	init_proc->currState = READY;	

	WriteRegister(REG_PTBR1, (unsigned int)init_proc->AddressSpace);
	WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
  	TracePrintf(0, "End of the init process </> \n\n\n");
        return init_proc;
}
