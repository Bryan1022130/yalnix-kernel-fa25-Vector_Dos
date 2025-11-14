//Header files from yalnix_framework
#include <sys/types.h> //for u_long
#include <load_info.h> //The struct for load_info
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC

//Our header files
#include "Queue.h"     
#include "process.h"   
#include "trap.h" 
#include "terminal.h"
#include "syscalls.h"
#include "lock.h"

//Extern Variables
extern unsigned long current_tick;
extern Queue *readyQueue;
extern Queue *blockedQueue;
extern PCB *current_process;
extern PCB *idle_process;
extern PCB *process_free_head;
extern unsigned char *track_global;
extern Terminal t_array[NUM_TERMINALS];
extern char *input_buffer;

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
	 
	int sys_return = 0;
	int extract_code = CurrUC->code;

	switch(extract_code){
		case YALNIX_FORK:
			sys_return = KernelFork();
			break;

		case YALNIX_EXEC:
			sys_return = KernelExec((char *)CurrUC->regs[0], (char **)CurrUC->regs[1]);
			break;
		
		case YALNIX_EXIT:
			KernelExit((int)CurrUC->regs[0]);
			break;

		case YALNIX_WAIT:
			sys_return = KernelWait((int *)CurrUC->regs[0]);
			break;

		case YALNIX_GETPID:
			sys_return = KernelGetPid();
			break;

		case YALNIX_BRK:
			sys_return = KernelBrk((void *)CurrUC->regs[0]);
			break;

		case YALNIX_DELAY:
			sys_return = KernelDelay((int)CurrUC->regs[0]);
			break;
		
		case YALNIX_TTY_READ:
			sys_return = KernelTtyRead((int)CurrUC->regs[0], (void *)CurrUC->regs[1], (int)CurrUC->regs[2]);
			break;

		case YALNIX_TTY_WRITE:
			sys_return = KernelTtyWrite((int)CurrUC->regs[0], (void *)CurrUC->regs[1], (int)CurrUC->regs[2]);
			break;

		case YALNIX_READ_SECTOR:
			sys_return = KernelReadSector((int)CurrUC->regs[0], (void *)CurrUC->regs[1]);
			break;

		case YALNIX_WRITE_SECTOR:
			sys_return = KernelWriteSector((int)CurrUC->regs[0], (void *)CurrUC->regs[1]);
			break;

		case YALNIX_PIPE_INIT:
			sys_return = KernelPipeInit((int *)CurrUC->regs[0]);
			break;

		case YALNIX_PIPE_READ:
			sys_return = KernelPipeRead((int)CurrUC->regs[0], (void *)CurrUC->regs[1], (int)CurrUC->regs[2]); 
			break;

		case YALNIX_PIPE_WRITE:
			sys_return = KernelPipeWrite((int)CurrUC->regs[0], (void *)CurrUC->regs[1], (int)CurrUC->regs[2]); 
			break;

		case YALNIX_LOCK_INIT:
			sys_return = KernelLockInit((int *)CurrUC->regs[0]); 
			break;

		case YALNIX_LOCK_ACQUIRE:
			sys_return = KernelAcquire((int)CurrUC->regs[0]); 
			break;

		case YALNIX_LOCK_RELEASE:
			sys_return = KernelRelease((int)CurrUC->regs[0]); 
			break;

		case YALNIX_CVAR_INIT:
			sys_return = KernelCvarInit((int *)CurrUC->regs[0]); 
			break;

		case YALNIX_CVAR_SIGNAL:
			sys_return = KernelCvarSignal((int)CurrUC->regs[0]); 
			break;

		case YALNIX_CVAR_BROADCAST:
			sys_return = KernelCvarBroadcast((int)CurrUC->regs[0]); 
			break;

		case YALNIX_CVAR_WAIT:
			sys_return = KernelCvarWait((int)CurrUC->regs[0], (int)CurrUC->regs[1]); 
			break;

		case YALNIX_RECLAIM:
			sys_return = KernelReclaim((int)CurrUC->regs[0]); 
			break;


		default:
			TracePrintf(0,"ERROR! The current code did not match any syscall\n");
			break;
	}

	//Store return value of syscall into regs[0] && UserContext
	CurrUC->regs[0] = sys_return;
	current_process->curr_uc.regs[0] = sys_return;
	*CurrUC = current_process->curr_uc;

	TracePrintf(0, "########## This is the return value of your syscall -> %d ############\n", sys_return);
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
    TracePrintf(0, "=================================================== Entering HandleClockTrap ===============================================\n");
    TracePrintf(0, "This is the pid of the process calling the clock trap --> %d\n", current_process->pid);
    current_process->curr_uc = *CurrUC;

    //Track clock traps for Delayy()
    current_tick++;

    // Save current UserContext into current process
    PCB *old_proc = current_process;
    memcpy(&old_proc->curr_uc, CurrUC, sizeof(UserContext));

    // wake up sleeping process when its time
    QueueNode *node = blockedQueue->head;
    QueueNode *prev = NULL;

    
    while (node != NULL) {
	PCB *p = (PCB *)node->data;
	QueueNode *next = node->next;

	// ckeck if its time to wake sleeping process
	if (p->wake_tick <= current_tick) {
	    TracePrintf(1, "ClockTrap: waking PID %d at tick %lu\n",p->pid, current_tick);

	    // remove from blockedQueue
	    if (prev == NULL)
		blockedQueue->head = next;
	    else
		prev->next = next;

	    if(next == NULL)
		blockedQueue->tail = prev;

	    blockedQueue->size--;

	    //mark proc ready and move to ready
	    p->currState = READY;
	    Enqueue(readyQueue, p);
	} else {
	    prev = node;
	}
	node = next;
    }

    //If no other process; enqueue the idle process
    if(idle_process == current_process && current_process->currState == READY){
	    Enqueue(readyQueue, current_process);
    } 

    //Check if there is node
    QueueNode *rnode = peek(readyQueue);
    if(rnode == NULL && current_process->currState == RUNNING){
	    TracePrintf(0,"There are no new ready processes!");
	    return;
    }

    //Extract the next PCB from the Queue
    PCB *next = get_next_ready_process();
    if(old_proc->currState == RUNNING){
	    TracePrintf(0, "I will be putting process %d into readyQueue and subbing in process %d\n", current_process->pid, next->pid);
	    //Add current process into readyQueue
	    current_process->currState = READY;
	    Enqueue(readyQueue, current_process);
	    next->currState = RUNNING;
    }

    if(next != old_proc){
	TracePrintf(0, "Switching from PID %d to PID %d\n", old_proc->pid, next->pid);
	if(KernelContextSwitch(KCSwitch, old_proc, next) == ERROR) { 
	  	  TracePrintf(0, "There was an error with Kernel context switch!\n");
		  return;
    	}
    }

    *CurrUC = current_process->curr_uc;
    TracePrintf(0, "========================================= Process %d leaving ClockTrap =======================================\n\n\n\n", current_process->pid);
    return;
}

void HandleIllegalTrap(UserContext *CurrUC){
	current_process->curr_uc = *CurrUC;

	TracePrintf(0, "IllegalTrap: You have invoked an Illegal Trap!" 
			"I will now abort this current process.\n");
	abort();

	*CurrUC = current_process->curr_uc;
}

/* =========================================
 * General Flow:
 * -> check if its in region 1
 * -> check if it's not going into heap memory (proc->current_user_brk)
 * -> grow the current user stack
 * -> update proc
 * -> flush && return
 * =========================================
 */

//TODO: CHECK WHY WHEN I DO A MEMSORY THAT IA VALID I GET AN ERROR
void HandleMemoryTrap(UserContext *CurrUC) {
    TracePrintf(0, "MemoryTrap: You have invoked a Memory Trap!\n");

    if(CurrUC->addr >= (void *)VMEM_1_BASE && CurrUC->addr < (void *)VMEM_1_LIMIT){
	    TracePrintf(0, "We are in current region 1 space. Nice!\n");
	    if (CurrUC->addr > current_process->user_heap_brk) {
		    TracePrintf(0, "This is the current_user_brk --> %p and this is the user addr --> %p\n", current_process->user_heap_brk, CurrUC->addr);
		    TracePrintf(0, "Great are able to grow the user stack space!\n");
		    TracePrintf(0, "I will now set up more stack memory for you.\n");

		    unsigned int curr_stack_base = (((unsigned long int)current_process->user_stack_ptr) >> PAGESHIFT);
		    unsigned int new_stack_base = (((unsigned long int) CurrUC->addr) >> PAGESHIFT) - MAX_PT_LEN;

		    TracePrintf(0, "Memory Trap: This is the current stack base --> %d\n", curr_stack_base);
		    TracePrintf(0, "Memory Trap: This is the new stack base --> %d\n", new_stack_base);

		    pte_t *reg1_pt = (pte_t *)current_process->AddressSpace;
		    //We store our region 1 space in our proc; so we just map it the same way as in template.c
		    for(unsigned int x = new_stack_base; x < curr_stack_base; x++){
			    int frame = find_frame(track_global);
			    if(frame == ERROR){
				    TracePrintf(0, "MemoryTrap: Error with finding a frame!\n");
				    goto die;
			    }			
			    TracePrintf(0, "MemoryTrap: This is the frame allocated for stack resize -> %d\n", frame);
			    reg1_pt[x].pfn = frame;
			    reg1_pt[x].valid = 1;
			    reg1_pt[x].prot = PROT_READ | PROT_WRITE;
		    }

		    //Update the stack base for the proc
		    TracePrintf(0, "MemoryTrap: Success! I will now return and stack has growed!\n");
		    current_process->user_stack_ptr = CurrUC->addr;
		    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
		    return;
    	}
    }

  die:
    //In we cant grow the stack then we should abort
    TracePrintf(0, "MemoryTrap: Error! I will now abort\n");
    abort();
}


/* =========================================
 * General Flow:
 * -> Abort()
 * =========================================
 */

void HandleMathTrap(UserContext *CurrUC) {
    TracePrintf(0, "MathTrap: Hello and Welcome to Math Trap!\n");
    TracePrintf(0, "I will call abort(). Please hold\n");

    abort();
}

/* =========================================
 * General Flow:
 * -> Index into CurrUC->code
 * -> Read the user input from terminal with TtyReceive
 * ---> Buffer if neccessary
 * -> Store in our terminal struct
 * -> This will get read later on by Ttyread
 * =========================================
 */

//Handle input here only
void HandleReceiveTrap(UserContext *CurrUC) {
    current_process->curr_uc = *CurrUC;
    TracePrintf(0, "================================= Hello this is ReceiveTrapHandler! ==================================\n");

    //extract the terminal number
    int terminal = CurrUC->code;

    MessageNode *read_node = calloc(1, sizeof(MessageNode));
    if (read_node == NULL) { 
	    TracePrintf(0, "HandleReceiveTrap: There was an error when creating a node for user input\n");
	    Halt();
    }

    //Alloc buff for recieve
    char *buffer_zero = calloc(1, TERMINAL_MAX_LINE);
    if (buffer_zero == NULL) {
	    TracePrintf(0, "HandleReceiveTrap: There was an error when creating buffer for receive\n");
	    Halt();
    }

    read_node->message = buffer_zero; //Store dynamic buffer in MessageNode
    int message_length = TtyReceive(terminal, (void *)buffer_zero, TERMINAL_MAX_LINE); //Read up to TERMINAL_MAX_LINE
    read_node->length = message_length; //Store message length

    if (message_length < TERMINAL_MAX_LINE) { 
	    TracePrintf(0, "HandleReceiveTrap: This the length of message_length --> %d\n", message_length);
	    realloc(buffer_zero, message_length); //Realloc the buffer to not waste memory 
	    TracePrintf(0, "HandleReceiveTrap: I just reallocated the buffer to the length above\n");

	    //Add node to the list of messages
	    if(read_add_message(terminal, read_node) == ERROR) {
		    free(read_node->message);
		    free(read_node);
		    TracePrintf(0, "HandleReceiveTrap: There was an error with read_add_message in HandleReceive!\n");
	    }

	    TracePrintf(0, "HandleReceiveTrap: Just to be safe here is the string ===> %s\n", read_node->message);
	    *CurrUC = current_process->curr_uc;
	    TracePrintf(0, "+++++++++++++++++++++++++++++++++++++ Leaving HandleReceiveTrap +++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	    return;
    }

    TracePrintf(0, "HandleReceiveTrap: We should have read a full TERMINAL_MAX_LINE input! This the length of message_length --> %d\n", message_length);
    if(read_add_message(terminal, read_node) == ERROR) {
	    free(read_node->message);
	    free(read_node);
	    TracePrintf(0, "There was an error with read_add_message in HandleReceive!\n");
	    return;
    }

    *CurrUC = current_process->curr_uc;
    TracePrintf(0, "+++++++++++++++++++++++++++++++++++++ Leaving HandleReceiveTrap +++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
}

/* =========================================
 * General Flow:
 * -> Index into CurrUC->code
 * -> Complete the blocked process that started the terminal
 * -> Check if there is more output and make sure it runs
 * =========================================
 */

void HandleTransmitTrap(UserContext *CurrUC) {
    TracePrintf(0, "======================================START TRANSMIT TRAP=========================================================\n");
    current_process->curr_uc = *CurrUC;

    TracePrintf(0, "TransmitTrap: Hello we are in the TransmitTrap handler!\n");
    TracePrintf(0,"This means that all your data from TTyTransmit has been written out!\n");

    int terminal = CurrUC->code;

    MessageNode *current_message = t_array[terminal].transmit_message_head;

    //We dont create a node for input < TERMINAL_MAX_LINE
    //So checking for if current_message == NULL is not correct
    //Thus we need to check if the waiting process == NULL also 
    if(current_message == NULL && t_array[terminal].transmit_waiting_process == NULL){
	    TracePrintf(0, "HandleTransmitTrap: ERROR!. The transit message head and waiting process are NULL!\n");
	    Halt();
    }

    //Logic if there is multiple messages
    if (current_message != NULL) {

	    TracePrintf(0, "Nice we transmitted this amount --> %d\n", current_message->length);

	    //Free the current_message malloced buffer
	    free(current_message->message);

	    //Move the to the next node and free the MessageNode
	    t_array[terminal].transmit_message_head = current_message->next;
	    free(current_message);

	    //Check if there is a blocked process
	    if (t_array[terminal].transmit_message_head != NULL) {

		    TracePrintf(0, "Perfect we have another messages to transmit!\n");
		    MessageNode *new_message = t_array[terminal].transmit_message_head;
		    TracePrintf(0, "This is the amount we are going to send --> %d\n", new_message->length);
		    TtyTransmit(terminal, (void *)new_message->message, new_message->length);
		    TracePrintf(0, "\n\nThis is farewell for now friend! Until the next interrupt :)\n");

		    *CurrUC = current_process->curr_uc;
		    TracePrintf(0, "======================================END TRANSMIT TRAP=========================================================\n");
		    return;
	    }
    }

    TracePrintf(0, "Hey we did it! All your messasge were sent!\n");
    PCB *wait_proc = t_array[terminal].transmit_waiting_process;
    if (wait_proc == NULL) {
	    TracePrintf(0, "ERROR! Why is the waiting process NULL! Check your TtyWrite logic\n");
	    Halt();
    }

    if (t_array[terminal].one_message_buffer != NULL) { 
	    TracePrintf(0,"Hello! This should only be true when we have a small message!\n");
	    TracePrintf(0,"Okay! I will now free the buffer!\n");
	    free(t_array[terminal].one_message_buffer);
	    
	    t_array[terminal].one_message_buffer = NULL;
	    t_array[terminal].one_message_length = 0;
    }

    //Now we can unblock the process
    TracePrintf(0, "We are unblocking process -> %d\n", wait_proc->pid);
    wait_proc->currState = READY;
    remove_data(blockedQueue, wait_proc);
    Enqueue(readyQueue, wait_proc);
    t_array[terminal].transmit_waiting_process = NULL;

    *CurrUC = current_process->curr_uc;
    TracePrintf(0, "======================================END TRANSMIT TRAP=========================================================\n");
}

void HandleDiskTrap(UserContext *CurrUC) {
    current_process->curr_uc = *CurrUC;

    TracePrintf(0, "TRAP: Disk called.\n");
    TracePrintf(0, "We are not handling this!");

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

//declaration of the default place holder function
void HandleTrap(UserContext *){
	int s = 0;
	while(s < 10){
		s++;
		TracePrintf(0, "You are currently in the place holder function for trap handler!\n");
	}
	return;
}

//Abort function
void abort(void){
	//TODO
	TracePrintf(0, "We are aborting the current process!\n");

	if (current_process->pid == 1){
		TracePrintf(0, "Idle is aborting! Time to Halt())\n");
		Halt();
	}

	//Get a new process to run 
	PCB *next = get_next_ready_process();
	if(next == NULL){
		TracePrintf(0, "There is no ready process. ERROR idle should be in Queue!\n");
		return;
	}

	//Free the proc from queues, and most of its memory
	free_proc(current_process, 0);

	//Context Switch
	if(KernelContextSwitch(KCSwitch, current_process, next) < 0){
		TracePrintf(0, "There was an error with KCS Switch!\n");
		return;
	}

	return;
}
