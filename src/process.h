//Enum to be able to represent the many states of the Process
typedef enum{
        NEW,
        READY,
        BLOCKED,
        RUNNING,
        TERMINATED,
        STOPPED,
	ZOMBIE
}ProcessState;

//Create the PCB that we be use for context switching
typedef struct PCB{

	//Kernel Stack Frames
	unsigned int kernel_stack_frames[KERNEL_STACK_MAXSIZE / PAGESIZE];

	//Context Switch Logic
        KernelContext curr_kc; // Hold the Current Kernel Context {Not just a pointer}
        UserContext curr_uc; //Hold the Current User Context { Not just the pointer}
	
	//Process information
        int pfn; // The physical frame numbers that are contained in the processes kernel stack {This could be a ssize_t}
	int ppid; //Parent Process id 
        int pid; //Keep track of the id of the current process {This could be changed to a ssize_t}
	int exitstatus; // When a process is going to exit

        ProcessState currState; //Enum states for process
        void *AddressSpace; // to keep track of where it is currently in its address space {Page Table}
	
	
	unsigned int user_heap_end; //The top of heap
	unsigned int user_stack_end; //Lowest stack address
	
	//For Process tracking, for sys calls such as wait() and exit()
	//Works as a stack and tree
	PCB *parent; //Parent Node
	PCB *first_child; //Child that was recently created by a fork() call
	PCB *next_sibling; //Points to the parent's next-oldest child 
	
	//Scheduling Logic
	unsigned long wake_tick; //For Delay() and time to wake up
        PCB *next;
        PCB *prev;

}PCB;


//Helper functions for Context Switching
KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p);

KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used);


