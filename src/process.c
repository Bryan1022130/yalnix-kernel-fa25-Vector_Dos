//Header files from yalnix_framework
#include <ykernel.h>
#include <hardware.h>
#include <ctype.h>
#include <load_info.h>
#include <yalnix.h>
#include <ylib.h>
#include <yuser.h>
#include "process.h"

// ----------------- Context Switching -----------------------------

KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p){
	// Save current kernel context into its PCB, restore the next PCB’s context,
	// and return pointer to the next context to run.

	//Check if the process is valid
	PCB *current_pcb = (PCB *)curr_pcb_p;
	PCB *next_pcb = (PCB *)next_pcb_p;

	if(current_pcb == NULL || next_pcb == NULL){
		TracePrintf(0, "Error with one of the PCB being NULL!");
	}

	//1. Copy over the current Contents of the KernelContext into the current PCB
	//1. This will be done using a memcpy into the PCB struct 
	
	//2. Return a Pointer to the KernelContext
	
}

KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used){
	// Copy kernel context (and later, kernel stack) for a newly forked process.
	// This produces a child that resumes right after Fork().

	//Check if the process is valid
	PCB *new_pcb = (PCB *)new_pcb_p;

	if(new_pcb == NULL){
		TracePrintf(0, "Error with one of the PCB being NULL!");
	}

	//1. Copy over the the KernelContext into the new_pcb with a memcpy
	
	//2.Copy over the content of the kernel stack into frames that have been allocated
	//2.This will be in a loop and using a memcpy
	
	
	return kc_in;
}
