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

//Macros
#define FALSE 0
#define TRUE 1

//Extern 
extern pte_t *kernel_page_table;
extern PCB *current_process;
extern UserContext *KernelUC;
extern Queue *readyQueue;
extern unsigned long int frame_count;

PCB *create_init_proc(pte_t *user_page_table, unsigned char *track, int track_size){
        TracePrintf(0, "Start of the init process </> \n");

        PCB *init_proc =(PCB *)malloc(sizeof(PCB));
        if(init_proc == NULL){
                TracePrintf(0, "Malloc error for init_proc\n");
		free(init_proc);
                return NULL;
        }

	memset(init_proc, 0, sizeof(PCB));

	//Get new pid with the help of the hardware
	init_proc->pid = helper_new_pid(user_page_table);
	TracePrintf(0, "This is the pid of the new process -> %d\n", init_proc->pid);

	// -------------------------------->> Setting up Region Table
	
        // Allocate a physical framem for the process
        int pt_pfn = find_frame(track, track_size);
         if(pt_pfn == ERROR) {
             TracePrintf(0, "Init Proc: Error with finding physical frame!\n");
	     free(init_proc);
             return NULL;
         }

        //Map the pfn into the the kernel_page_table
	int stack_find = MAX_PT_LEN - 2;
        kernel_page_table[stack_find].pfn = pt_pfn;
        kernel_page_table[stack_find].prot = PROT_READ | PROT_WRITE;
        kernel_page_table[stack_find].valid = TRUE;

        TracePrintf(0, "Flushing Region 1\n");
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
	
	//Point to region 1 page table
        pte_t *init_pt = (pte_t *)user_page_table;
	memset(init_pt, 0, (MAX_PT_LEN * sizeof(pte_t)));

	//Set up its Kernel Frames
	int kf_ret = create_sframes(init_proc, track, frame_count);
	if(kf_ret == ERROR){
		TracePrintf(0, "Init Proc: Error with setting up kernel stack!\n");
		frame_free(track, pt_pfn);
		free(init_proc);
		return NULL;
	}
	
	//Set up proces informatio
	init_proc->AddressSpace = (void *)user_page_table;
	
	//Put the kernel contect into init_proc
	int copy = KernelContextSwitch(KCCopy, init_proc, NULL);
	if(copy < 0){
		TracePrintf(0, "Error with Kernel Context Switch\n");
		return NULL;
	}

	//a UserContext (from the the uctxt argument to KernelStart))
	memcpy(&init_proc->curr_uc, KernelUC, sizeof(UserContext));
	init_proc->currState = READY;	

	WriteRegister(REG_PTBR1, (unsigned int)init_proc->AddressSpace);
	WriteRegister(REG_PTLR1, (unsigned int)MAX_PT_LEN);
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
	
	//Queue the process into the Queue {Maybe this wrong}
//	Enqueue(readyQueue,(void *)init_proc); 


	// ==================== INIT PROCESS DEBUG START ====================
	TracePrintf(1, "\nINIT_PROC DEBUG: Verifying Context and Stack Setup:\n");

	// --- PCB Contexts ---
	TracePrintf(1, "  INIT PID: %d\n", init_proc->pid);
	TracePrintf(1, "  INIT PCB AddressSpace (PTBR1): 0x%p\n", init_proc->AddressSpace);
	TracePrintf(1, "  INIT PCB currState: %d (Should be READY)\n", init_proc->currState);

	// --- Kernel Stack Frames (Crucial for KCCopy source) ---
	TracePrintf(1, "  INIT Kernel Stack Frame 0 (PFN): %d\n", init_proc->kernel_stack_frames[0]);
	TracePrintf(1, "  INIT Kernel Stack Frame 1 (PFN): %d\n", init_proc->kernel_stack_frames[1]);

	// --- User Context (The starting point for User Mode) ---
	// If these are (nil) or garbage, the system crashes on return to user mode.
	TracePrintf(1, "  INIT User Context PC: 0x%p (Should be USER_TEXT_START)\n", init_proc->curr_uc.pc);
	TracePrintf(1, "  INIT User Context SP: 0x%p (Should be USER_STACK_LIMIT)\n", init_proc->curr_uc.sp);
	TracePrintf(1, "  INIT User Context R0 (Fork RetVal): %d\n", init_proc->curr_uc.regs[REG_SS]);

	TracePrintf(1, "  INIT Kernel Context Address: 0x%p\n", &init_proc->curr_kc);

  	TracePrintf(0, "End of the init process </> \n");
        return init_proc;
}

void init(void){
	TracePrintf(0, ">>> init() started <<<\n");
	int x = 0;
	while(x < 10){
		x++;
		TracePrintf(0, "THIS WILL PRINT FOREVER\n");
	}
}
