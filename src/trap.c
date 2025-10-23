//Header files from yalnix_framework
#include <ykernel.h>
#include <hardware.h>
#include <ctype.h>
#include <load_info.h>
#include <yalnix.h>
#include <ylib.h>
#include <yuser.h>
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC
#include "trap.h"

//typedef for function pointer that will point to the Handle Trap function
typedef void (*HandleTrapCall)(UserContext *CurrUC);

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
			Exit(CurrUC->regs[0]);
			break;

		case YALNIX_WAIT:
			sys_return = Wait(CurrUC->regs[0]);
			break;

		case YALNIX_GETPID:
			sys_return = GetPid();
			break;

		case YALNIX_BRK:
			sys_return = Brk(CurrUC->regs[0]);
			break;

		case YALNIX_DELAY:
			sys_return = Delay(CurrUC->regs[0]);
			break;
		
		case YALNIX_TTY_READ:
			sys_return = TtyRead(CurrUC->regs[0], CurrUC->regs[1], CurrUC->regs[2]);
			break;

		case YALNIX_TTY_WRITE:
			sys_return = TtyWrite(CurrUC->regs[0], CurrUC->regs[1], CurrUC->regs[2]);
			break;

		case YALNIX_READ_SECTOR:
			sys_return = ReadSector(CurrUC->regs[0], CurrUC->regs[1]);
			break;

		case YALNIX_WRITE_SECTOR:
			sys_return = WriteSector(CurrUC->regs[0], CurrUC->regs[1]);
			break;

		case YALNIX_PIPE_INIT:
			sys_return = PipeInit(CurrUC->regs[0]);
			break;

		case YALNIX_PIPE_READ:
			sys_return = PipeRead(CurrUC->regs[0], CurrUC->regs[1], CurrUC->regs[2]); 
			break;

		case YALNIX_PIPE_WRITE:
			sys_return = PipeWrite(CurrUC->regs[0], CurrUC->regs[1], CurrUC->regs[2]); 
			break;

		default:
			PrintTracef(0,"The current code did not match any syscall");
			break;

	//Store the value that we get from the syscall into the regs[0];
	CurrUC->regs[0] = sys_return;

	TracePrintf(0, "Leaving HandleKernelTrap");

	return;
	}
}

/* <<<---------------------------------
 * General Flow
 *  ->
 * --------------------------------->>>
 */

void HandleClockTrap(UserContext *CurrUC){
	TracePrintf(0, "In HandleClockTrap");

	TracePrintf(0, "Leaving HandleKernelTrap");
	return;
}


void setup_trap_handler(HandleTrapCall Interrupt_Vector_Table[]){

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
}


