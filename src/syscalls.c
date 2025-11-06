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
extern pte_t *kernel_page_table;
extern unsigned char *track_global;
extern unsigned long frame_count;
extern Queue *readyQueue;
extern PCB *idle_process;

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
     *  1. Allocate a new PCB.
     *  2. Copy parent's address space and kernel/user contexts.
     *  3. Set child->state = READY and add to scheduler queue.
     *  4. Return child's PID to parent, and 0 to child.
     *
     */

    TracePrintf(1, "Fork called by pid %d\n", current_process->pid);

    PCB *child = (PCB *)malloc(sizeof(PCB));
    if (!child) return ERROR;

    // --- duplicate address space ---
    pte_t *parent_pt = (pte_t *)current_process->AddressSpace;

    // allocate page table frame for child
    int child_pt_pfn = find_frame(track_global, frame_count);
    if (child_pt_pfn == ERROR) return ERROR;

    // temporarily map it in kernel region 0
    int free_vpn = (KERNEL_STACK_BASE >> PAGESHIFT) - 2;
    kernel_page_table[free_vpn].pfn = child_pt_pfn;
    kernel_page_table[free_vpn].valid = 1;
    kernel_page_table[free_vpn].prot = PROT_READ | PROT_WRITE;
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    pte_t *child_pt = (pte_t *)((free_vpn) << PAGESHIFT);

    // clear entries
    memset(child_pt, 0, MAX_PT_LEN * sizeof(pte_t));

    // copy valid parent pages
    for (int vpn = 0; vpn < MAX_PT_LEN; vpn++) {
        if (!parent_pt[vpn].valid) continue;
        int pfn = find_frame(track_global, frame_count);
        if (pfn == ERROR) return ERROR;

        // map parent + child pages temporarily
        int tmp_parent = free_vpn - 1;
        int tmp_child  = free_vpn - 3;
        kernel_page_table[tmp_parent].pfn = parent_pt[vpn].pfn;
        kernel_page_table[tmp_parent].valid = 1;
        kernel_page_table[tmp_parent].prot = PROT_READ;

        kernel_page_table[tmp_child].pfn = pfn;
        kernel_page_table[tmp_child].valid = 1;
        kernel_page_table[tmp_child].prot = PROT_READ | PROT_WRITE;
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

        memcpy((void *)(tmp_child << PAGESHIFT),
               (void *)(tmp_parent << PAGESHIFT), PAGESIZE);

        // unmap
        kernel_page_table[tmp_parent].valid = 0;
        kernel_page_table[tmp_child].valid = 0;

        // fill child PT
        child_pt[vpn].pfn = pfn;
        child_pt[vpn].valid = 1;
        child_pt[vpn].prot = parent_pt[vpn].prot;
    }

    kernel_page_table[free_vpn].valid = 0;
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

    // record child address space
    child->AddressSpace = child_pt;

    // duplicate contexts
    KCCopy(&current_process->curr_kc, child, NULL);
    memcpy(&child->curr_uc, &current_process->curr_uc, sizeof(UserContext));
    memcpy(&child->curr_kc, &current_process->curr_kc, sizeof(KernelContext));

    child->curr_uc.regs[0] = 0;
    current_process->curr_uc.regs[0] = child->pid;

    // parent/child linkage
    child->parent = current_process;
    child->next_sibling = current_process->first_child;
    current_process->first_child = child;

    child->currState = READY;
    Enqueue(readyQueue, child);

    TracePrintf(1, "Fork success, child pid %d\n", child->pid);
    return child->pid;
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
    TracePrintf(1, "Exec called by pid %d for %s\n", current_process->pid, filename);

    if (filename == NULL) return ERROR;

    // free old region1 frames
    pte_t *pt = (pte_t *)current_process->AddressSpace;
    for (int i = 0; i < MAX_PT_LEN; i++) {
        if (pt[i].valid) frame_free(track_global, pt[i].pfn);
        pt[i].valid = 0;
    }

    // load new program
    int result = LoadProgram(filename, argv, current_process);
    if (result == ERROR) return ERROR;

    TracePrintf(1, "Exec success -> now running new program %s\n", filename);
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

    TracePrintf(1, "Process %d exiting with status %d\n", current_process->pid, status);
    TracePrintf(1, "This is exit syscall\n");
    current_process->exitstatus = status;
    current_process->currState = ZOMBIE;

    // wake waiting parent if any
    if (current_process->parent && current_process->parent->currState == BLOCKED) {
        Enqueue(readyQueue, current_process->parent);
        current_process->parent->currState = READY;
    }

    PCB *next = get_next_ready_process();
    
    if (next == idle_process && next->curr_kc.lc.uc_mcontext.gregs[REG_ESP] == 0) {
        TracePrintf(0, "Exit: Cannot switch to idle (never run). Halting system.\n");
        Halt();  // No other process to run, system should halt
    }
    
    KernelContextSwitch(KCSwitch, current_process, next);
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
    PCB *child = current_process->first_child;

    if (child == NULL) {
        TracePrintf(0, "Wait(): no children\n");
        return ERROR;
    }

    // look for zombie children
    while (child != NULL) {
        if (child->currState == ZOMBIE) {
            if (status_ptr != NULL) *status_ptr = child->exitstatus;
            int pid = child->pid;
            //proc_free(child);
            return pid;
        }
        child = child->next_sibling;
    }

    // if none are terminated, block until clock trap wakes us
    current_process->currState = BLOCKED;
    Enqueue(sleepQueue, current_process);
    KernelContextSwitch(KCSwitch, current_process, get_next_ready_process());
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

    // put it on a "sleep queue"
    Enqueue(sleepQueue, current_process);

    // Get next ready process
    PCB *next = get_next_ready_process();

    // ✅ DON'T SWITCH TO IDLE IF IT'S NEVER RUN BEFORE
    if (next != NULL && next != current_process) {
        // Check if next is idle and has never run
        if (next == idle_process && next->curr_kc.lc.uc_mcontext.gregs[REG_ESP] == 0) {
            TracePrintf(0, "Delay: Can't switch to idle - it's never run! Continuing with current.\n");
            current_process->currState = READY;  // Unblock ourselves
            Dequeue(sleepQueue);  // Remove from sleep queue
        } else {
            KernelContextSwitch(KCSwitch, current_process, next);
        }
    } else {
        TracePrintf(0, "Delay: no other process ready, continuing with current\n");
        current_process->currState = READY;
        Dequeue(sleepQueue);
    }

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
    if (tty_id < 0 || tty_id >= NUM_TERMINALS || buf == NULL || len <= 0)
        return ERROR;

    char *msg = (char *)buf;
    
    TtyTransmit(tty_id, buf, len);

    return len;
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

int LockInit(int *lock_idp) {
    return 0;
}

int Acquire(int lock_id) {
    return 0;
}

int Release(int lock_id) {
    return 0;
}

int CvarInit(int *cvar_idp) {
    return 0;
}

int CvarSignal(int cvar_id) {
    return 0;
}

int CvarBroadcast(int cvar_id) {
    return 0;
}

int CvarWait(int cvar_id, int lock_id) {
    return 0;
}

int Reclaim(int lock_id) {
    return 0;
}
