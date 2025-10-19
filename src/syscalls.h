#pragma once
#include "process.h"
#include "memory.h"

/*
 * System Call Interface

 * Each user-level system call (like Fork, Exec, Exit, Wait) eventually
 * triggers a TRAP_KERNEL interrupt that is handled here.
 
 * These functions are invoked from the trap handler.
 * They manipulate PCBs, memory, and the scheduler to perform the
 * requested kernel services.
 */

// Process-related syscalls
int sys_fork(void);
int sys_exec(char *filename, char *argv[]);
void sys_exit(int status);
int sys_wait(int *status_ptr);

// Memory-related syscalls
int sys_brk(void *addr);

