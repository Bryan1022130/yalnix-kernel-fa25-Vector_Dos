// ------------------ PROCESS LOGIC ---------------------------------

//Enum to be able to represent the many states of the Process
typedef enum{
	NEW,
	READY,
	BLOCKED,
	RUNNING,
	TERMINATED,
	STOPPED,
}ProcessState;

//Create the PCB that we be use for context switching 
typedef struct{
	KernelContext CurrKC; // Hold the Current Kernel Context {Not just a pointer}
	UserContext CurrUC; //Hold the Current User Context { Not just the pointer}
	int Running; //To check if the process is running or not
	int PFN; // The physical frame numbers that are contained in the processes kernel stack {This could be a ssize_t}
	int pid; //Keep track of the id of the current process {This could be changed to assize_t}
	ProcessState CurrState;
	void *AddressSpace; // to keep track of where it is currently in its address space {Page Table}
	int ExitStatus; // When a process is going to exit
	
	//Linked List Data Structure for storing and accessing PCB blocks
	PCB *next;
	PCB *prev;

}PCB;

// Keep track of the CurrentProcess; global variable to pass to other functions
PCB *CurrentProcess = NULL;

// ----------------- Context Switching -----------------------------


KernelContext *KCSwitch( KernelContext *kc_in, void *curr pcb_p, void *next pcb_p);

KernelContext *KCCopy( KernelContext *kc_in, void *new_pcb p, void *not_used)

