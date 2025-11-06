//Header files from yalnix_framework
#include <sys/types.h> //For u_long
#include <load_info.h> //The struct for load_info
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL
#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC

//Our header files
#include "Queue.h"     
#include "process.h"   
#include "trap.h" 

//Extern Variables
extern unsigned long current_tick;
extern Queue *readyQueue;
extern Queue *sleepQueue;
extern PCB *current_process;
extern PCB *idle_process;
extern PCB *process_free_head;

//declaration of the default place holder function
void HandleTrap(UserContext *CurrUC){
	int s = 0;
	while(s < 10){
		s++;
		TracePrintf(0, "You are currently in the place holder function for trap handler!\n");
	}
	return;
}

//Abort function
void abort(void){
	TracePrintf(0, "We are aborting the current process!");
	
	//Get the next process that can run 
	PCB *next = get_next_ready_process();

	free_proc(current_process);

	//Context Switch
	if(KernelContextSwitch(KCSwitch, NULL, next) < 0){
		TracePrintf(0, "There was an error with KCS Switch!\n");
		return;
	}

	return;
}
//On the way into a trap, your kernel should copy the incoming user context at that address to the PCB of the current process.
//On the way out of a trap, your kernel should copy user context in the PCB of the current process (which may have changed) to that address.

/* <<<---------------------------------
 * General Flow
 * #include <yalnix.h>
 *  -> Index in CurrUC->code;
 *  -> exec syscall 
 *  -> regs[] contains args to syscall
 *  -> return value is stored in reg[0]
 * --------------------------------->>>
 */

void HandleKernelTrap(UserContext *CurrUC){
	TracePrintf(0, "In HandleKernelTrap Function ===========================================================\n");
	current_process->curr_uc = *CurrUC;
	
	//Set up the variable that will be the return value 
	int sys_return = 0;
	int extract_code = CurrUC->code;

	//Find what Syscall the code points to 
	switch(extract_code){
		case YALNIX_FORK:
			sys_return = Fork();
			break;

		case YALNIX_EXEC:
			sys_return = Exec((char *)CurrUC->regs[0], (char **)CurrUC->regs[1]);
			break;
		
		case YALNIX_EXIT:
			TracePrintf(0, "This is HandleKernelTrap and we are calling Exit()\n");
			Exit((int)CurrUC->regs[0]);
			break;

		case YALNIX_WAIT:
			sys_return = Wait((int *)CurrUC->regs[0]);
			break;

		case YALNIX_GETPID:
			sys_return = GetPid();
			break;

		case YALNIX_BRK:
			sys_return = Brk((void *)CurrUC->regs[0]);
			break;

		case YALNIX_DELAY:
			sys_return = Delay((int)CurrUC->regs[0]);
			break;
		
		case YALNIX_TTY_READ:
			sys_return = TtyRead((int)CurrUC->regs[0], (void *)CurrUC->regs[1], (int)CurrUC->regs[2]);
			break;

		case YALNIX_TTY_WRITE:
			sys_return = TtyWrite((int)CurrUC->regs[0], (void *)CurrUC->regs[1], (int)CurrUC->regs[2]);
			break;

		case YALNIX_READ_SECTOR:
			sys_return = ReadSector((int)CurrUC->regs[0], (void *)CurrUC->regs[1]);
			break;

		case YALNIX_WRITE_SECTOR:
			sys_return = WriteSector((int)CurrUC->regs[0], (void *)CurrUC->regs[1]);
			break;

		case YALNIX_PIPE_INIT:
			sys_return = PipeInit((int *)CurrUC->regs[0]);
			break;

		case YALNIX_PIPE_READ:
			sys_return = PipeRead((int)CurrUC->regs[0], (void *)CurrUC->regs[1], (int)CurrUC->regs[2]); 
			break;

		case YALNIX_PIPE_WRITE:
			sys_return = PipeWrite((int)CurrUC->regs[0], (void *)CurrUC->regs[1], (int)CurrUC->regs[2]); 
			break;

		case YALNIX_LOCK_INIT:
			sys_return = LockInit((int *)CurrUC->regs[0]); 
			break;

		case YALNIX_LOCK_ACQUIRE:
			sys_return = Acquire((int)CurrUC->regs[0]); 
			break;

		case YALNIX_LOCK_RELEASE:
			sys_return = Release((int)CurrUC->regs[0]); 
			break;

		case YALNIX_CVAR_INIT:
			sys_return = CvarInit((int *)CurrUC->regs[0]); 
			break;

		case YALNIX_CVAR_SIGNAL:
			sys_return = CvarSignal((int)CurrUC->regs[0]); 
			break;

		case YALNIX_CVAR_BROADCAST:
			sys_return = CvarBroadcast((int)CurrUC->regs[0]); 
			break;

		case YALNIX_CVAR_WAIT:
			sys_return = CvarWait((int)CurrUC->regs[0], (int)CurrUC->regs[1]); 
			break;

		case YALNIX_RECLAIM:
			sys_return = Reclaim((int)CurrUC->regs[0]); 
			break;


		default:
			TracePrintf(0,"ERROR! The current code did not match any syscall\n");
			break;
	}

	//Store the value that we get from the syscall into the regs[0];
	CurrUC->regs[0] = sys_return;
	*CurrUC = current_process->curr_uc;
	TracePrintf(0, "Leaving HandleKernelTrap =====================================================================\n");
	return;
}

/* <<<---------------------------------
 * General Flow: In Progress for now
 * CP1 only needs use to trace print that it works
 *  ->
 * --------------------------------->>>
 */

void HandleClockTrap(UserContext *CurrUC){
    current_process->curr_uc = *CurrUC;
    TracePrintf(0, "\n\n");

    //Increment how many ticks
    current_tick++;

    // Save current user context
    PCB *old_proc = current_process;
    memcpy(&old_proc->curr_uc, CurrUC, sizeof(UserContext));

    if(idle_process == current_process && current_process->currState == READY){
	    Enqueue(readyQueue, current_process);
    } 

    //Check if there is node
    QueueNode *node = peek(readyQueue);
    TracePrintf(0, "Dequeued node: %p\n", node);
    if(node == NULL && current_process->currState == RUNNING){
	    TracePrintf(0,"There is no new process that is ready!");
	    return;
    }

    //Extract the next PCB from the Queue
    PCB *next = get_next_ready_process();
    if(old_proc->currState == RUNNING){
	    TracePrintf(0, "THE CURRENT PROCESS WAS RUNNING SETTING TO THE READY QUEUE\n");
	    current_process->currState = READY;
	    Enqueue(readyQueue, current_process);
    }

    next->currState = RUNNING;
    if(next != old_proc){
	TracePrintf(0, "Switching from PID %d to PID %d\n", old_proc->pid, next);
    	int kcs = KernelContextSwitch(KCSwitch, old_proc, next);
    	if(kcs == ERROR){
	  	  TracePrintf(0, "There was an error with Kernel context switch!\n");
		  return;
    	}
    }
        
    // Load new process's user context
    memcpy(CurrUC, &current_process->curr_uc, sizeof(UserContext));
    *CurrUC = current_process->curr_uc;
    TracePrintf(0, "Leaving HandleClockTrap ============================================================================\n\n\n\n");
    return;
}

void HandleIllegalTrap(UserContext *CurrUC) {
	current_process->curr_uc = *CurrUC;

	TracePrintf(0, "IllegalTrap: You have invoked an Illegal Trap!" 
			"I will now abort this current process.\n");
	abort();

	*CurrUC = current_process->curr_uc;
}

void HandleMemoryTrap(UserContext *CurrUC) {
    current_process->curr_uc = *CurrUC;

    TracePrintf(0, "MemoryTrap: You have invoked a Memory Trap!\n");

    Halt();	
    *CurrUC = current_process->curr_uc;
}

void HandleMathTrap(UserContext *CurrUC) {
    current_process->curr_uc = *CurrUC;
    TracePrintf(0, "TRAP: Math error! Halting.\n");
    Halt();
    *CurrUC = current_process->curr_uc;
}

void HandleReceiveTrap(UserContext *CurrUC) {
    current_process->curr_uc = *CurrUC;
    TracePrintf(0, "TRAP: TTY Receive called.\n");
    *CurrUC = current_process->curr_uc;
}

void HandleTransmitTrap(UserContext *CurrUC) {
    current_process->curr_uc = *CurrUC;
    TracePrintf(0, "TRAP: TTY Transmit called.\n");	
    *CurrUC = current_process->curr_uc;
}

void HandleDiskTrap(UserContext *CurrUC) {
    TracePrintf(0, "TRAP: Disk called.\n");
    *CurrUC = current_process->curr_uc;
}

/* <<<----------------------------------------------
 *  Function to create the Interrupt Vector Table
 *  -> Status: Working for now 
 * ---------------------------------------------->>>
 */
void setup_trap_handler(HandleTrapCall Interrupt_Vector_Table[]){
	TracePrintf(0, "We are setting up the Interrupt vector table\n");

	//Setup the table with default func
	for(int i = 0; i < TRAP_VECTOR_SIZE; i++){
		Interrupt_Vector_Table[i] = &HandleTrap;
	}

	Interrupt_Vector_Table[TRAP_KERNEL] = &HandleKernelTrap;
	Interrupt_Vector_Table[TRAP_CLOCK] = &HandleClockTrap;
  	Interrupt_Vector_Table[TRAP_ILLEGAL] = &HandleIllegalTrap;
    	Interrupt_Vector_Table[TRAP_MEMORY] = &HandleMemoryTrap;
    	Interrupt_Vector_Table[TRAP_MATH] = &HandleMathTrap;
    	Interrupt_Vector_Table[TRAP_TTY_RECEIVE] = &HandleReceiveTrap;
    	Interrupt_Vector_Table[TRAP_TTY_TRANSMIT] = &HandleTransmitTrap;
    	Interrupt_Vector_Table[TRAP_DISK] = &HandleDiskTrap;

	//Write the Memory Address to the REG_VECTOR_BASE register so it knows where it is
	WriteRegister(REG_VECTOR_BASE, (unsigned int)Interrupt_Vector_Table);

	TracePrintf(0,"We just finshed writing the Interrupt Vector Table\n");
}


