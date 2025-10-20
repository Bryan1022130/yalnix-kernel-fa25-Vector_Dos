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
	int PFN; // The physical frame numbers that are contained in the processes kernel stack {This could be a ssize_t}
	int pid; //Keep track of the id of the current process {This could be changed to a ssize_t}
	ProcessState CurrState; //Enum states
	void *AddressSpace; // to keep track of where it is currently in its address space {Page Table}
	int ExitStatus; // When a process is going to exit
	
	//Linked List Data Structure for storing and accessing PCB blocks
	PCB *next;
	PCB *prev;

}PCB;

// Keep track of the CurrentProcess; global variable to pass to other functions
// Extern Maybe
PCB *CurrentProcess = NULL;

// ----------------- Context Switching -----------------------------

KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p){
	//Check if the process is valid
	PCB *current_pcb = (PCB *)curr_pcb_p;
	PCB *next_pcb = (PCB *)next_pcb_pl;

	if(current_pcb == NULL || next_pcb == NULL){
		TracePrintf(0, "Error with one of the PCB being NULL!");
	}

	//1. Copy over the current Contents of the KernelContext into the current PCB
	//1. This will be done using a memcpy into the PCB struct 
	
	//2. Return a Pointer to the KernelContext
	
}

KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used){
	//Check if the process is valid
	PCB *new_pcb = (PCB *)new_pcb_p;

	if(new_pcb == NULL)	{
		TracePrintf(0, "Error with one of the PCB being NULL!");
	}

	//1. Copy over the the KernelContext into the new_pcb with a memcpy
	
	//2.Copy over the content of the kernel stack into frames that have been allocated
	//2.This will be in a loop and using a memcpy
	
	
	return kc_in;
}
