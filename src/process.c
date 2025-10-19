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

// ------------------ TRAP HANDLERS ---------------------------------

//typedef for function pointer that will point to the Handle Trap function
typedef void (*HandleTrapCall)(UserContext *CurrUC, int TrapValue);

//Array of function pointers for handling traps
HandleTrapCall Interrupt_Vector_Table[TRAP_VECTOR_SIZE];

//Setup the table!
for(int i = 0; i < TRAP_VECTOR_SIZE; i++){
	Interrupt_Vector_Table[i] = &HandleTrap;
}

//Write the Memory Address to the REG_VECTOR_BASE register so it knows where it is
WriteRegister(REG_VECTOR_BASE, (unsigned int)&Interrupt_Vector_Table);


//Find the correct case for the Trap type, then call a function to handle it
void HandleTrap(UserContext *CurrUC, int TrapValue){

	//Copy the incoming user context into the PCB 
	CopyUserContext();
	
	switch(TrapValue){

		case TRAP_KERNEL:
			HandleKernelTrap(CurrUC);
			break;

		case TRAP_CLOCK:
			HandleClockTrap(CurrUC);
			break;

		case TRAP_ILLEGAL:
			HandleIllegalTrap(CurrUC);
			break;
		case TRAP_MEMORY:
			HandleMemoryTrap(CurrUC);
			break;
		
		case TRAP_MATH: 
			HandleMathTrap(CurrUC);
			break;

		case TRAP_TTY_RECEIVE:
			HandleReceiveTrap(CurrUC);
			break;

		case TRAP_TTY_TRANSMIT:
			HandleTransmitTrap(CurrUC);
			break;

		case TRAP_DISK:
			HandleDiskTrap(CurrUC);
			break;

		default:
			PrintTracef(0, "That was an invalid TrapValue");
			break;
	}

	UpdateUserContext();

}

// Function declarations for Trap Handler functions
void HandleKernelTrap(UserContext *CurrUC);
void HandleMathTrap(UserContext *CurrUC);
void HandleClockTrap(UserContext *CurrUC);
void HandleMemoryTrap(UserContext *CurrUC);
void HandleIllegalTrap(UserContext *CurrUC);
void HandleDiskTrap(UserContext *CurrUC);
void HandleTransmitTrap(UserContext *CurrUC);
void HandleReceiveTrap(UserContext *CurrUC);

//These functions will use a memcopy into the global PCB
void CopyUserContext();
void UpdateUserContext();


