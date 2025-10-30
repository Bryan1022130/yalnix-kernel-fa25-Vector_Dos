#include <hardware.h>

//This is code for out init process
// Bryan: Handle Brk, GetPid, Delay
// For now our KernelStart is the idle process and it cloning into init
//
//
// What does out PCB need?
// Its region 1 page table
// new frames for its kernel stack frames {I think we did this already in PCB Alloc}
// a UserContext (from the the uctxt argument to KernelStart)) {I think we did this already, but we can just do a memcpy}
// a pid (from helper new pid() )
//
// Angel:  Write KCCopy() function this is in proceces.h and process.c
// Angel: Fix any of the other things we left unsued in check point 1
// in this function we also need to flush since it keeps track of different address
// KCCopy is currently done right now ------- :) 


void init(void){
	while(1){
		TracePrintf(0, "This called when there is no arguments passed to ./yalnix\n");
	}

}


