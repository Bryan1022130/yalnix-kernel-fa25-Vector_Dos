#include "syscalls.h"
#include "process.h"
#include "Queue.h"
#include <yalnix.h>
#include <hardware.h>
#include <ykernel.h>

// extern globals from kernelstart.c
extern PCB *current_process;
extern Queue *sleepQueue;
extern unsigned long current_tick;

int GetPid(void) {
    if (current_process == NULL) {
        TracePrintf(0, "GetPid called but current_process NULL\n");
        return ERROR;
    }
    return current_process->pid;
}

int Fork(void) {
    /*
     * GOAL:
     *  Create a new process that is a copy of the current one.
     *
     * STEPS:
     *  1. Allocate a new PCB via proc_alloc().
     *  2. Copy parent's address space and kernel/user contexts.
     *  3. Set child->state = READY and add to scheduler queue.
     *  4. Return child's PID to parent, and 0 to child.
     *
     */

    return 0;
}
int Exec(char *filename, char *argv[]) {
    /*
     * GOAL:
     *  Replace current process’s memory image with a new program.

     *  1. Reclaim current memory frames (Region 1) via frame_free().
     *  2. Load executable 'filename' into new address space.
     *  3. Reset user stack and registers (via LoadProgram()).
     *  4. If successful, never return (process continues in new program).
     *  5. If failed, return -1.
     */
    return 0;
}

void Exit(int status) {
    /*
     * GOAL:
     *  Terminate the current process and notify parent.
     *
     *  1. Set CurrentProcess->exit_status = status.
     *  2. Change state to ZOMBIE.
     *  3. Wake up any waiting parent process.
     *  4. Call scheduler to switch to next runnable process.
     */
}

int Wait(int *status_ptr) {
    /*
     * GOAL:
     *  Wait for a child process to exit and collect its status.
     *
     *  1. If no children alive → return ERROR.
     *  2. If child in ZOMBIE state:
     *       - Copy child->exit_status into *status_ptr.
     *       - Free child’s PCB via proc_free().
     *       - Return child PID.
     *  3. Else, block the calling process until child exits.
     */
    return 0;
}

int Brk(void *addr) {
    /*
     * GOAL:
     *  Adjust end of data segment for the calling process.
     *
     *  1. Call SetKernelBrk(addr) if in kernel.
     *  2. Or adjust process heap in Region 1 via vm subsystem.
     *  3. Return 0 on success, ERROR otherwise.
     */
     TracePrintf(1, "sys_brk called: %p\n", addr);

    // get current process
    PCB *proc = current_process;
    if (proc == NULL) return ERROR;

    // check address sanity
    if (addr == NULL) return ERROR;

    // For now, pretend success and record where the heap ends
    proc->user_heap_end = (unsigned int)addr;

    // later (CP4) you’ll map/unmap frames here
    return 0;
}

int Delay(int clock_ticks) {
    if (clock_ticks <= 0) return 0;

    current_process->wake_tick = current_tick + clock_ticks;
    current_process->currState = BLOCKED;

    // put it on a "sleep queue" (you can use your Queue)
    Enqueue(sleepQueue, current_process);

    // switch to another process (scheduler)
    KernelContextSwitch(KCSwitch, current_process, get_next_ready_process());
    return 0;
}
/*
int Fork(void) {
    TracePrintf(0, "Fork()\n");
    return ERROR;
}

int Exec(char *filename, char **argv) {
    TracePrintf(0, "Exec()\n");
    return ERROR;
}

void Exit(int status) {
    TracePrintf(0, "Exit()\n");
    Halt();
}

int Wait(int *status_ptr) {
    TracePrintf(0, "Wait()\n");
    return ERROR;
}
/*
int GetPid(void) {
    TracePrintf(0, "GetPid()\n");
    return ERROR;
}

int Brk(void *addr) {
    TracePrintf(0, "Brk()\n");
    return ERROR;
}

int Delay(int clock_ticks) {
    TracePrintf(0, "Delay()\n");
    return ERROR;
}
*/

int TtyRead(int tty_id, void *buf, int len) {
    TracePrintf(0, "TtyRead()\n");
    return ERROR;
}

int TtyWrite(int tty_id, void *buf, int len) {
    TracePrintf(0, "TtyWrite()\n");
    return ERROR;
}

int ReadSector(int sector, void *buf) {
    TracePrintf(0, "ReadSector()\n");
    return ERROR;
}

int WriteSector(int sector, void *buf) {
    TracePrintf(0, "WriteSector()\n");
    return ERROR;
}

int PipeInit(int *pipe_id_ptr) {
    TracePrintf(0, "PipeInit()\n");
    return ERROR;
}

int PipeRead(int pipe_id, void *buf, int len) {
    TracePrintf(0, "PipeRead()\n");
    return ERROR;
}

int PipeWrite(int pipe_id, void *buf, int len) {
    TracePrintf(0, "PipeWrite()\n");
    return ERROR;
}
