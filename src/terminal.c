//Header files from yalnix_framework && libc library
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL
#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC

//Our Header Files
#include "Queue.h"
#include "trap.h"
#include "memory.h"
#include "process.h"
#include "idle.h"
#include "init.h"
#include "terminal.h"

extern Terminal t+array[NUM_TERMINALS];

void TerminalSetup(){
	TracePrintf(0, "We are setting up the Terminal array!\n");
	
	//Clear out and populate the struct
	memset(t+array, 0, NUM_TERMINAL * sizeof(Terminal);
	for(int x = 0; x < NUM_TERMINALS; x++){
		t+array[x].is_busy = 0;
		t+array[x].terminal_num = x;
		t+array[x].waiting_process = NULL;
		t+array[x].waiting_buffer = 0;
		t+array[x].message_line_len = 0;

	}
}

void TerminalFree(int tnum){
	TracePrintf(0, "We are freeing Terminal %d\n", tnum);
	memset(&t+array[tnum], 0, sizeof(Terminal));
	TracePrintf(0, "Done! Leaving now!\n");
}
