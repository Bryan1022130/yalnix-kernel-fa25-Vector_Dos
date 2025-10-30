#pragma once
#include <hardware.h>

//Interrupt Vector Table function declarations
typedef void (*HandleTrapCall)(UserContext *CurrUC);
void setup_trap_handler(HandleTrapCall Interrupt_Vector_Table[]);

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

//Temporary function for filling in the table
void HandleTrap(UserContext *CurrUC);
