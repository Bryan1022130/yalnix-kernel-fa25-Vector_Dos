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

/* Trap Handlers for Check Point 2
 * -> TRAP_CLOCK
 * -> TRAP_KERNEL
 */

void HandleClockTrap(UserContext *CurrUC){
	TracePrintf(0, "In HandleClockTrap");

	TracePrintf(0, "Leaving HandleKernelTrap");
	return;
}

void HandleKenelTrap(UserContext *CurrUC){
	TracePrintf(0, "In HandleKernelTrap");

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


