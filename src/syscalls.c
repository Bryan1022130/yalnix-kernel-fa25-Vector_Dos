#include "syscalls.h"
#include "process.h"
#include "Queue.h"
#include "terminal.h"
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
extern Terminal t_array[NUM_TERMINALS];

int KernelGetPid(void) {
    if (current_process == NULL) {
        TracePrintf(0, "GetPid called but current_process NULL\n");
        return ERROR;
    }
    return current_process->pid;
}

int KernelFork(void) {
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

int KernelExec(char *filename, char *argv[]) {
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

void KernelExit(int status) {
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
    current_process->exit_status = status;
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

int KernelWait(int *status_ptr) {
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
            if (status_ptr != NULL) *status_ptr = child->exit_status;
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

int KernelBrk(void *addr) {

     TracePrintf(1, "KernelBrk: requested addr=%p\n", addr);

    // get current process
    PCB *proc = current_process;

    if (!proc) return ERROR;

    // check address sanity
    if (!addr) return ERROR;

    // For now, pretend success and record where the heap ends
    //proc->user_heap_brk = (void *)addr;

    // Converting to integers
    uintptr_t new_brk_req = (uintptr_t)addr; // requested break
    uintptr_t old_brk = (uintptr_t)proc->user_heap_brk; //currrent brk

    // page rounding 
    uintptr_t new_brk = (new_brk_req + (PAGESIZE - 1)) & ~(uintptr_t)(PAGESIZE - 1);

    // Pointers 
    pte_t *pt = (pte_t *)proc->AddressSpace;

    // computing stack boundary
    uintptr_t stack_base_vpn = ((uintptr_t)proc->user_stack_ptr) >> PAGESHIFT;

    // check is brk makes sense 
    // is the brk above or below the current brk and or base?
    if (new_brk < (uintptr_t)VMEM_1_BASE) return ERROR; // cannot be bellow region 1
    if (old_brk < (uintptr_t)VMEM_1_BASE) old_brk = (uintptr_t)VMEM_1_BASE;

    int old_vpn = (int)((old_brk - VMEM_1_BASE) >> PAGESHIFT);
    int new_vpn = (int)((new_brk - VMEM_1_BASE) >> PAGESHIFT);

    // leaving one page  between heap and stack 
    // Cannot grow to or past (stack_base_vpn -1)
    if (new_vpn >= (int)stack_base_vpn - 1) {
	TracePrintf(0, "KernelBrk: would collide with stack (new_vpn=%d, stack_base_vpn=%lu)\n",
	new_vpn, (unsigned long)stack_base_vpn);
	return ERROR;
    }

    if (new_brk == old_brk) return 0;

    // Growing and shinking heap
    if (new_brk > old_brk){
    // grow heap
	int start_vpn = old_vpn; 
	int end_vpn = new_vpn - 1;

	TracePrintf(1, "KernelBrk: grow heap from vpn %d to %d\n", start_vpn, end_vpn);

	for (int vpn = start_vpn; vpn<= end_vpn; vpn++){
		// we skip pages already valid
		if (pt[vpn].valid) continue;

		int pfn = find_frame(track_global, frame_count);
		if (pfn == ERROR){
			TracePrintf(0, "KernelBrk: OOM while growing at vpn=%d\n", vpn);
			// roll page pages we just mapped
			for (int r = start_vpn; r < vpn; r++){
				if (pt[r].valid){
					frame_free(track_global, pt[r].pfn);
					pt[r].pfn = 0;
					pt[r].valid = 0;
					pt[r].prot = 0;
				}
			}
			return ERROR;
		}
		// mapping read and write for user heap
		pt[vpn].pfn = pfn;
		pt[vpn].valid = 1;
		pt[vpn].prot = (PROT_READ | PROT_WRITE);
	}
	// shrinking heap
	}else {

	int start_vpn = new_vpn; //first page to free
	int end_vpn = old_vpn -1; // last page to free

	TracePrintf(1, "KernelBrk: shrinking heap from vpn %d down to %d\n", end_vpn, start_vpn);

	for(int vpn = start_vpn; vpn <= end_vpn; vpn++){
		if (!pt[vpn].valid)continue; //nothing to free if invalid 

		// freeing physical frame and clear PTE
		frame_free(track_global, pt[vpn].pfn);
		pt[vpn].pfn = 0;
		pt[vpn].valid = 0;
		pt[vpn].prot = 0;
	}
    }
    // flush
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    proc->user_heap_brk = (void *)new_brk;
    TracePrintf(1, "KernelBrk: success, new brk=%p\n", proc->user_heap_brk);

    // later (CP4) you’ll map/unmap frames here
    return 0;
}

int KernelDelay(int clock_ticks) {

    TracePrintf(1, "KernelDelay: called by PID %d with %d ticks\n",
	current_process->pid, clock_ticks);

    // check if tick makes sense
    // tick cannot be less than 0
    if (clock_ticks < 0) {
	TracePrintf(0, "KernelDelay: invalid tick count (<0)\n");
	return ERROR;
    }

    // if 0 clock has not started
    if( clock_ticks == 0) {
	TracePrintf(1, "KernelDelay: tick count = 0, returning\n");
	return 0;
    }

    // Records wake up time
    // clock trap increments current_tick very tick
    // ordered by wake tick in sleep queue moved to ready queue when ready
    current_process->wake_tick = current_tick + clock_ticks;

    // block current proccess and put it in sleep queue
    current_process->currState = BLOCKED;
    Enqueue(sleepQueue, current_process); // add proc to sleep queue
    TracePrintf(1, "KernelDelay: PID %d sleeping unitil tick %lu\n",
	current_process->pid, current_process->wake_tick);

    // Get next ready process
    // Context switch, picks next ready process, idle if none
    PCB *next = get_next_ready_process();

    if (next == NULL){
	TracePrintf(0, "KernelDelay: no other ready process, continuing with idle\n");
	next = idle_process;
    }

    KernelContextSwitch(KCSwitch, current_process, next);
    TracePrintf(1, "KernelDelay: PID %d resumed after delay\n",
		    current_process->pid);
    return 0;
}
/* ============================
 * I/O Syscalls
 * ============================
 */

int KernelTtyRead(int tty_id, void *buf, int len){
	if (tty_id < 0 || tty_id >= NUM_TERMINALS || buf == NULL || len <= 0){
		TracePrintf(0, "One of your arguments was invalid!\n");
		return ERROR;
	}

	//Call TtyReceive
	int length = TtyReceive(tty_id, buf, len);

	return length;
}


int KernelTtyWrite(int tty_id, void *buf, int len){
	if (tty_id < 0 || tty_id >= NUM_TERMINALS || buf == NULL || len <= 0){
		TracePrintf(0, "One of your arguments was invalid!\n");
		return ERROR;
	}

	if(t_array[tty_id].waiting_process == current_process){
		TracePrintf(0, "Sorry but you are waiting for transit to finish! PLEASE WAIT!\n");
		return ERROR;
	}

	//The current process is waiting until a the Transmit Trap to be free
	t_array[tty_id].waiting_process = current_process;

	//If len is under TERMINAL_MAX_LINE or equal
	if(len <= TERMINAL_MAX_LINE){
		TracePrintf(0, "We can transmit your message with one call!\n");
		TtyTransmit(tty_id, buf, len);
		TracePrintf(0, "Transmit was done and returing to you this much -> %d", len);
		return len;
	}

	TracePrintf(0, "Looks like we are going to transmit your message with many cycles!\n");
	TracePrintf(0, "For debug purposes this is the amount that len is -> %ld\n");

	char *temp_buf = malloc(TERMINAL_MAX_LINE * sizeof(char));
	if(temp_buf == NULL){
		TracePrintf(0, "There was an error with malloc in KernelTtyWrite()");
		return ERROR;
	}

	//clear malloced space
	memset(temp_buf, 0, sizeof(TERMINAL_MAX_LINE * sizeof(char)));

	//--------------------------------------------------- Support logic for when len > TERMINAL_MAX_LINE
	
	unsigned long int bytes_left = len;
	char *buffer_start = (char *)buf;
	int copy_amount = TERMINAL_MAX_LINE;

	while(bytes_left > 0){
		TracePrintf(0, "INFO: This is the amount of bytes left -->%ld", bytes_left);
		copy_amount = (bytes_left > TERMINAL_MAX_LINE) ? TERMINAL_MAX_LINE : bytes_left;
		TracePrintf(0, "INFO: This is the amount of bytes that will be copied -->%ld", copy_amount);

		//Copy from the current start of buffer into the temporary buffer
		memcpy((void *)temp_buf, (void *)buffer_start, copy_amount);

		//Call Transmit hardware function
		TtyTransmit(tty_id, temp_buf, copy_amount);

		//Move the buffer pointer to next place to copy
		buffer_start += copy_amount;
		bytes_left -= copy_amount;
	}

	//free the temp buf
	free(temp_buf);
	TracePrintf(0, "Great we are done with the KernelTtyWrite syscall! Bye!\n");
	return len;
}

/* ============================
 * Other Syscalls?
 * ============================
 */

int KernelReadSector(int sector, void *buf) {
    TracePrintf(0, "ReadSector()\n");
    return ERROR;
}

int KernelWriteSector(int sector, void *buf) {
    TracePrintf(0, "WriteSector()\n");
    return ERROR;
}

/* ============================
 * IPC Syscalls
 * ============================
 */

int KernelPipeInit(int *pipe_id_ptr) {
    TracePrintf(0, "PipeInit()\n");
    return ERROR;
}

int KernelPipeRead(int pipe_id, void *buf, int len) {
    TracePrintf(0, "PipeRead()\n");
    return ERROR;
}

int KernelPipeWrite(int pipe_id, void *buf, int len) {
    TracePrintf(0, "PipeWrite()\n");
    return ERROR;
}

/* ============================
 * Synchronization Syscalls
 * ============================
 */

int KernelLockInit(int *lock_idp) {
    return 0;
}

int KernelAcquire(int lock_id) {
    return 0;
}

int KernelRelease(int lock_id) {
    return 0;
}

int KernelCvarInit(int *cvar_idp) {
    return 0;
}

int KernelCvarSignal(int cvar_id) {
    return 0;
}

int KernelCvarBroadcast(int cvar_id) {
    return 0;
}

int KernelCvarWait(int cvar_id, int lock_id) {
    return 0;
}

int KernelReclaim(int lock_id) {
    return 0;
}
