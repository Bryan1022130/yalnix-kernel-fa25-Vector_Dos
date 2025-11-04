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

/* ===============================================================================================================
 * pcb_alloc()
 * Returns a pointer to the first FREE PCB in the table.
 * Sets the state to READY and assigns its PID.
 * ===============================================================================================================
 */
PCB *proc_alloc(void) {
	//This is wrong
    static int next_pid = 1;  // simple incremental pid generator

    if (process_free_head == NULL) return NULL;
    PCB *p = process_free_head;
    process_free_head = p->next;
    memset(p, 0, sizeof(PCB));
    p->currState = READY;
    p->pid = next_pid++;  // assign pid here

    return p;
}

/* ===============================================================================================================
 * pcb_free(pid)
 * Frees all resources associated with a process:
 *  - Unmaps Region 1 frames
 *  - Frees its kernel stack frames
 *  - Resets PCB state to FREE
 * ===============================================================================================================
 */

void proc_free(PCB *p) {
    if (!p) return;
    p->currState = FREE;
    p->next = process_free_head;
    process_free_head = p;
}

// ----------------- Context Switching -----------------------------
PCB *get_next_ready_process(void) {
    if (isEmpty(readyQueue)) {
        return idle_process;
    }
    return Dequeue(readyQueue);
}

//declaration of the default place holder function
void HandleTrap(UserContext *CurrUC){
	int s = 0;
	while(s < 10){
		s++;
		TracePrintf(0, "You are currently in the place holder function for trap handler!\n");
	}
	return;
}

/* <<<---------------------------------
 * General Flow
 *  -> Index in CurrUC->code;
 *  -> exec syscall 
 *  -> regs[] contains args to syscall
 *  -> return value is stored in reg[0]
 * --------------------------------->>>
 */

void HandleKernelTrap(UserContext *CurrUC){
	TracePrintf(0, "In HandleKernelTrap Function\n");
	
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

		default:
			TracePrintf(0,"ERROR! The current code did not match any syscall\n");
			break;
	}
	//Store the value that we get from the syscall into the regs[0];
	CurrUC->regs[0] = sys_return;

	TracePrintf(0, "Leaving HandleKernelTrap\n");

	return;
}

/* <<<---------------------------------
 * General Flow: In Progress for now
 * CP1 only needs use to trace print that it works
 *  ->
 * --------------------------------->>>
 */

void HandleClockTrap(UserContext *CurrUC){
    TracePrintf(0, "In HandleClockTrap ==================================================================== \n");
    if(current_process->pid == 0){TracePrintf(0, "********************************************************** This is the IDLE PROCESS (CURRENT)\n");}
    else{TracePrintf(0, "********************************************************** This is the INIT PROCESS (CURRENT)\n");}
    TracePrintf(0, "Queue size: %d\n", readyQueue->size);
    
    //Increment how many ticks
    current_tick++;

    // Save current user context
    PCB *old_proc = current_process;
    memcpy(&old_proc->curr_uc, CurrUC, sizeof(UserContext));
    current_tick++;

    if(idle_process == current_process && current_process->currState == READY){
	    Enqueue(readyQueue, current_process);
    } 

    //Check if there is node
    QueueNode *node = peek(readyQueue);
    if(node == NULL && current_process->currState == RUNNING){
	    TracePrintf(0,"There is no new process that is ready!");
	    return;
    }

    TracePrintf(0, "This is the pid of next - > %d, and this is queue->next -> %d\n", (PCB *)node->data, (PCB *)node->next->data);

    //Extract the next PCB from the Queue
    PCB *next = idle_process;
	    //get_next_ready_process();
    if(next->pid == 0){
	    TracePrintf(0, "THIS IS THE IDLE PROCESS AND WE ARE GOING TO DEQUEU IT AND THIS IS THE PID -> %d\n", next->pid);
	 TracePrintf(0, "THIS IS THE CURRENT PROCESSS -> %d\n", current_process->pid);

    } else{
	    TracePrintf(0, "THIS IS THE INIT PROCESS AND WE ARE GOING TO DEQUEU IT\n");
    }



    if(old_proc->currState == RUNNING){
	    TracePrintf(0, "THE CURRENT PROCESS WAS RUNNING SETTING TO THE READY QUEUE\n");
	    current_process->currState = READY;
	    Enqueue(readyQueue, current_process);
    }

    next->currState = RUNNING;
    TracePrintf(0, "Hello this is to check right before the kernel contect switch\n");
    //if(next != old_proc){
    	// Switch kernel contexts
	TracePrintf(0, "Switching from PID %d to PID %d\n", old_proc->pid, idle_process->pid);
    	int kcs = KernelContextSwitch(KCSwitch, old_proc, next);
    	if(kcs == ERROR){
	  	  TracePrintf(0, "There was an error with Kernel context switch!\n");
		  return;
    	}
    //}
        
    // Load new process's user context
    //memcpy(CurrUC, &current_process->curr_uc, sizeof(UserContext));
	*CurrUC = current_process->curr_uc;
    TracePrintf(0, "Leaving HandleClockTrap ============================================================================\n\n\n\n");
    return;
}

void HandleIllegalTrap(UserContext *CurrUC) {
    TracePrintf(0, "TRAP: Illegal instruction! Halting.\n");
    Halt();
}

void HandleMemoryTrap(UserContext *CurrUC) {
    TracePrintf(0, "TRAP: Memory error! Halting.\n");
    Halt();
}

void HandleMathTrap(UserContext *CurrUC) {
    TracePrintf(0, "TRAP: Math error! Halting.\n");
    Halt();
}

void HandleReceiveTrap(UserContext *CurrUC) {
    TracePrintf(0, "TRAP: TTY Receive called.\n");
}

void HandleTransmitTrap(UserContext *CurrUC) {
    TracePrintf(0, "TRAP: TTY Transmit called.\n");
}

void HandleDiskTrap(UserContext *CurrUC) {
    TracePrintf(0, "TRAP: Disk called.\n");
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


