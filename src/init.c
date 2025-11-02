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

//Ask the hardware for a pid and let it know a new process is being spawned
extern pte_t *kernel_page_table;
extern PCB *current_process;
extern UserContext *KernelUC;
extern Queue *readyQueue;
extern unsigned long int frame_count;


//This should be process 2 
PCB *create_init_proc(pte_t *user_page_table, unsigned char *track, int track_size){
        TracePrintf(0, "We are creating the init process {This should be process 2}\n");
	TracePrintf(0, "If we atleast get this message than that means we are in a good place :)\n");

        PCB *init_proc =(PCB *)malloc(sizeof(PCB));
        if(init_proc == NULL){
                TracePrintf(0, "Error with Malloc for init_pcb\n");
                return NULL;
        }

	// -------------------------------->> Setting up Region Table
	//Get new pid with the help of the hardware
	init_proc->pid = helper_new_pid(user_page_table);
	TracePrintf(0, "This is the pid of the new process -> %d\n", init_proc->pid);

        // Allocate a physical framem for the process
        int pt_pfn = find_frame(track, track_size);
         if (pt_pfn == ERROR) {
             TracePrintf(0, "idle_proc_create(): ERROR allocating PT frame\n");
             return NULL;
         }

        //Map the pfn into the the kernel_page_table
	int stack_find = MAX_PT_LEN - 2;
        kernel_page_table[stack_find].pfn = pt_pfn;
        kernel_page_table[stack_find].prot = PROT_READ | PROT_WRITE;
        kernel_page_table[stack_find].valid = TRUE;

        TracePrintf(0, "Flushing Region 1\n");
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
	
	/*
	//Point to region 1 page table
        pte_t *init_pt = (pte_t *)user_page_table;
	memset(init_pt, 0, sizeof(MAX_PT_LEN * sizeof(pte_t)));

	//clear out the region one page table and make it invalid
	for(int x = 0; x < MAX_PT_LEN; x++){
		init_pt[x].prot = 0;
		init_pt[x].valid = FALSE;
		init_pt[x].pfn= 0;
	}
	*/

	//Set up its Kernel Frames
	create_sframes(init_proc, track, frame_count);
	
	//Copy over info to new init_proc
	init_proc->AddressSpace = (void *)user_page_table;
	KCCopy(&current_process->curr_kc, init_proc, NULL);

	//Set up information for the PCB
	memcpy(&init_proc->curr_uc, KernelUC, sizeof(UserContext)); // Copy in the curr UserContext
	init_proc->currState = READY;	
	init_proc->parent = NULL;
	init_proc->first_child = NULL;
	init_proc->next_sibling = NULL;
	init_proc->wake_tick = 0;

	WriteRegister(REG_PTBR1, (unsigned int)init_proc->AddressSpace);
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
	
	//Queue the process into the Queue {Maybe this wrong}
	//Enqueue(readyQueue,(void *)init_proc); 
	
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
