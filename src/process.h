#pragma once
#include <hardware.h>
typedef struct pcb PCB;
#include "memory.h"

//Enum to be able to represent the many states of the Process
typedef enum{
        FREE,
	SLEEP,
        READY,
        BLOCKED,
        RUNNING,
        TERMINATED,
        STOPPED,
	ZOMBIE
}ProcessState;

//Create the PCB that we be use for context switching
struct pcb{

	//Kernel Stack Frames
	unsigned int kernel_stack_frames[KERNEL_STACK_MAXSIZE / PAGESIZE];

	//Context Switch Logic
        KernelContext curr_kc; // Hold the Current Kernel Context {Not just a pointer}
        UserContext curr_uc; //Hold the Current User Context { Not just the pointer}
	
	//Process information
        unsigned int pid; //Keep track of the id of the current process {This could be changed to a ssize_t}
	int exit_status; // When a process is going to exit

        ProcessState currState; //Enum states for process
        pte_t *AddressSpace; // to keep track of where it is currently in its address space for region 1 {Page Table}
		
	// TODO: make these page indices
	void *user_heap_brk; //The top of heap
	void *user_stack_ptr; //Lowest stack address
	
	//For Process tracking, for sys calls such as wait() and exit()
	//Works as a stack and tree
	PCB *parent; //Parent Node
	PCB *first_child; //Child that was recently created by a fork() call
	PCB *next_sibling; //Points to the parent's next-oldest child 
	
	//Scheduling Logic
	unsigned long wake_tick; //For Delay() and time to wake up
        PCB *next;
        PCB *prev;

};

//Helper functions for Context Switching
KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p);
KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used);

//Process allocation functions
PCB *spawn_proc(void);
void free_proc(PCB *proc, int free_flip);
PCB *get_next_ready_process(void);
