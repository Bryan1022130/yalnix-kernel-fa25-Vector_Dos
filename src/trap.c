//Header files from yalnix_framework
#include <sys/types.h>
#include <ctype.h>
#include <load_info.h>
#include <ykernel.h>
#include <hardware.h>
#include <yalnix.h>
#include <ylib.h>
#include <yuser.h>
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC

#include "trap.h"

//Declaration of the default place holder function
void HandleTrap(UserContext *CurrUC){
	TracePrintf(0, "This is a placeholder function\n");
	return;
}

/*|==================================|
 *| Trap Handlers for Check Point 2  |
 *| -> TRAP_CLOCK		     |
 *| -> TRAP_KERNEL		     |
 *|==================================|
 */

/* <<<---------------------------------
 * General Flow
 *  -> Index in CurrUC->code;
 *  -> exec syscall 
 *  -> regs[] contains args to syscall
 *  -> return value is stored in reg[0]
 * --------------------------------->>>
 */

void HandleKernelTrap(UserContext *CurrUC){
	TracePrintf(0, "In HandleKernelTrap");
	
	//Get the code from the struct
	int extract_code = CurrUC->code;

	//Set up the variable that will be the return value 
	int sys_return = 0;

	//Find what Syscall the code points to 
	switch(extract_code){
		case YALNIX_FORK:
			sys_return = Fork();
			break;

		case YALNIX_EXEC:
			sys_return = Exec((char *)CurrUC->regs[0], (char **)CurrUC->regs[1]);
			break;
		
		case YALNIX_EXIT:
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
			TracePrintf(0,"The current code did not match any syscall");
			break;

	//Store the value that we get from the syscall into the regs[0];
	CurrUC->regs[0] = sys_return;

	TracePrintf(0, "Leaving HandleKernelTrap");

	return;
	}
}

/* <<<---------------------------------
 * General Flow: In Progress for now
 * CP1 only needs use to trace print that it works
 *  ->
 * --------------------------------->>>
 */

void HandleClockTrap(UserContext *CurrUC){
	TracePrintf(0, "In HandleClockTrap");

	TracePrintf(0, "Leaving HandleKernelTrap");
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

void setup_trap_handler(HandleTrapCall Interrupt_Vector_Table[]){
	TracePrintf(0, "We are setting up the Interrupt vector table\n");
	//Setup the table!
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


