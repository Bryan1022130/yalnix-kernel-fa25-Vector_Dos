//Header files from yalnix_framework && libc library
#include <sys/types.h> //For u_long
#include <load_info.h> //The struct for load_info
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC

//Our Header Files
#include "memory.h" 
#include "process.h"

//Extern 
extern UserContext *KernelUC;

PCB *create_init_proc(unsigned char *track) {
        TracePrintf(0, "Start of the init process </> \n");

        PCB *init_proc = calloc(1, sizeof(PCB));
        if (init_proc == NULL) {
                TracePrintf(0, "Calloc error for init_proc\n");
                return NULL;
        }
	
	//Set up its Kernel Stack Frames
	int kf_ret = create_sframes(init_proc, track);
	if (kf_ret == ERROR) {
		TracePrintf(0, "Init Proc: Error with setting up kernel stack!\n");
		free(init_proc);
		return NULL;
	}
	
	//Malloc new region 1 space
	pte_t *proc_space = (pte_t *)calloc(1, (MAX_PT_LEN * sizeof(pte_t)));
	if (proc_space == NULL) {
		TracePrintf(0,"There was an error with malloc for region 1 space");
		free(init_proc);
		return NULL;
	}

	init_proc->AddressSpace = proc_space;
	init_proc->pid = helper_new_pid(proc_space);

	//a UserContext (from the the uctxt argument to KernelStart
	memcpy(&init_proc->curr_uc, KernelUC, sizeof(UserContext));
	init_proc->currState = READY;	

	WriteRegister(REG_PTBR1, (unsigned int)init_proc->AddressSpace);
	WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

  	TracePrintf(0, "End of the init process </> \n\n\n");
        return init_proc;
}
