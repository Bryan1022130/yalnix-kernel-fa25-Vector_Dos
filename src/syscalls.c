#include "syscalls.h"
#include "traps.h"




int sys_fork(void) {
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
int sys_exec(char *filename, char *argv[]) {
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

void sys_exit(int status) {
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

int sys_wait(int *status_ptr) {
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

int sys_brk(void *addr) {
    /*
     * GOAL:
     *  Adjust end of data segment for the calling process.
     *
     *  1. Call SetKernelBrk(addr) if in kernel.
     *  2. Or adjust process heap in Region 1 via vm subsystem.
     *  3. Return 0 on success, ERROR otherwise.
     */
    return 0;
}
