//Header files from yalnix_framework
#include <ykernel.h>
#include <hardware.h>
#include <ctype.h>
#include <load_info.h>
#include <yalnix.h>
#include <ylib.h>
#include <yuser.h>
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC
#include "Queue.h"

//typedef for function pointer that will point to the Handle Trap function
typedef void (*HandleTrapCall)(UserContext *CurrUC);

void setup_trap_handler(void){

	//Array of function pointers for handling traps
	HandleTrapCall Interrupt_Vector_Table[TRAP_VECTOR_SIZE];

	//Setup the table!
	for(int i = 0; i < TRAP_VECTOR_SIZE; i++){
		Interrupt_Vector_Table[i] = &HandleTrap;
	}

	//Write the Memory Address to the REG_VECTOR_BASE register so it knows where it is
	WriteRegister(REG_VECTOR_BASE, (unsigned int)&Interrupt_Vector_Table);
}

//Find the correct case for the Trap type, then call a function to handle it
void HandleTrap(UserContext *CurrUC){

        //Copy the incoming user context into the PCB
        CopyUserContext();
	
	//The code field in the CurrUC is used to index into the interrupt table
        switch(CurrUC-> code){
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

