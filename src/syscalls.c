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
extern PCB *init_process;

//Helper Functions
void rollback_frames(int first_frame, int amount){
	TracePrintf(0, "We are rolling back stack frames becauses there was an error!\n");
	for(int x = first_frame; x < amount; x++){
		frame_free(track_global, x);
	}
	TracePrintf(0, "We are done rolling back the stack frames. Leaving! Bye!\n");
}

void data_copy(void *parent_pte, int cpfn){
	//This space is allocated in memory.c
	int temp_vpn_kernel = 125;

	//Store in kernel memory so that we can memcpy
	kernel_page_table[temp_vpn_kernel].pfn = cpfn;
	kernel_page_table[temp_vpn_kernel].valid = 1;
	kernel_page_table[temp_vpn_kernel].prot = (PROT_READ | PROT_WRITE);


	void *kernel_ptr = (void *)(temp_vpn_kernel << PAGESHIFT);

	//Flush so that new table entry is recongnized
	WriteRegister(REG_TLB_FLUSH, (unsigned int)kernel_ptr);

	//Copy the data from parent in to kernel_ptr (our our child)
	memcpy(kernel_ptr, parent_pte, PAGESIZE);

	//Clear out the space
	kernel_page_table[temp_vpn_kernel].pfn = 0;
	kernel_page_table[temp_vpn_kernel].valid = 0;
	kernel_page_table[temp_vpn_kernel].prot = 0;

	WriteRegister(REG_TLB_FLUSH, (unsigned int)kernel_ptr);
}

//Syscall Functions
int KernelGetPid(void){
    if (current_process == NULL) {
        TracePrintf(0, "GetPid called but current_process NULL\n");
        return ERROR;
    }
    TracePrintf(0, "KernelGetPid(): This is the current pid number -> %d\n", current_process->pid);
    return current_process->pid;
}

int KernelFork(void){
    TracePrintf(0, "==========================================================================================\n");
    TracePrintf(0, "This is the KernelFork() function and this is the current pid -> %d\n", current_process->pid);

    PCB *parent = current_process;
    PCB *child = spawn_proc();
    if(child == NULL){
	    TracePrintf(0, "There was an error making a process in KernelFork()\n");
	    return ERROR;
    }
 
    //Set up info needed for fork
    int child_pid = child->pid;
    pte_t *child_reg1 = (pte_t *)child->AddressSpace;
    pte_t *parent_reg1 = (pte_t *)parent->AddressSpace;

    int frames_used = 0;
    int first_frame_used = 0;
    int used = 0;
    int return_value = 0;

    //Loop through parent region 1 space and copying over to child
    for (int vpn = 0; vpn < MAX_PT_LEN; vpn++){
	    if (!parent_reg1[vpn].valid) continue;

	    int pfn = find_frame(track_global, frame_count);
	    //If there is an error free allocated frames and free the proc
	    if(pfn == ERROR){
		    rollback_frames(first_frame_used, frames_used);
		    free_proc(child);
		    return ERROR;
	    }else if(used == 0){
		    first_frame_used = pfn;
		    used = 1;
	    }

	    frames_used++;

	    //We have the pfn, we need to cycle through each parents page table entry
	    //Copy over page
	    //Note:  This might have to be the macro for VMEM_1_BASE
	    void *parent_pte_find = (void *)((vpn + ((unsigned int )VMEM_1_BASE >> PAGESHIFT)) << PAGESHIFT);
	    TracePrintf(0, "We are going to copy the data over right now!\n");

	    data_copy(parent_pte_find, pfn);

	    //set up new page table entries for child region 1 page table
	    child_reg1[vpn].pfn = pfn;
	    child_reg1[vpn].prot = parent_reg1[vpn].prot;
	    child_reg1[vpn].valid = 1;

    }

    //Set up other information
    child->ppid = parent->pid;
    child->parent = parent;
    memcpy(&child->curr_uc, &parent->curr_uc, sizeof(UserContext));
    child->user_heap_brk = parent->user_heap_brk;
    child->user_stack_ptr = parent->user_stack_ptr;
    child->exit_status = 0;

    //Update fork children tracking information
    child->next_sibling = parent->first_child;
    parent->first_child = child;
    child->first_child = NULL;
    
    //KCCopy to copy info from parent to child
    int rc = KernelContextSwitch(KCCopy, child, NULL);
    if(rc == ERROR){
	    TracePrintf(0, "There was an error with KernelContextSwitch for child process on Fork()\n");
	    return ERROR;
    }

    if(parent->pid != child_pid){
	    TracePrintf(0, "I am the parent process! Just to make sure here is my pid -> %d\n", parent->pid);
	    //The parent returns the child pid 
	    return child_pid;

    }else{
	    TracePrintf(0, "I am the child process! Just to make sure here is my pid -> %d\n", parent->pid);
	    return 0;
    }

    //If this is returned than we have an error
    return ERROR;
}

int KernelExec(char *filename, char *argv[]) {
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
}

void KernelExit(int status) {

    TracePrintf(1, "Process %d exiting with status %d\n", current_process->pid, status);
    TracePrintf(1, "This is exit syscall\n");

    // save exit status
    current_process->exit_status = status;
    // Mark proc as finished but not freed
    current_process->currState = ZOMBIE;

    // making orphans (setting their parent to init PID 1
    PCB *child = current_process->first_child;
    while (child != NULL){
        child->parent = init_process;
        child = child->next_sibling;
    }

    if (current_process->parent != NULL) {
        PCB *parent = current_process->parent;

        if (parent->currState == BLOCKED) {
            TracePrintf(2, "KernelExit: waking parent PID %d\n", parent->pid);
            parent->currState = READY;
            Enqueue(readyQueue, parent);
        }
    }

    // Pick next proc to run
    PCB *next_proc = get_next_ready_process();

    if(next_proc == NULL){
        TracePrintf(0, "KernelExit: no more runnable process, halting system.\n");
        Halt();
        return;
    }

    // context switch from dying proc
    TracePrintf(1, "KernelExit: switching from PID %d to PID %d\n", current_process->pid, next_proc->pid);
    KernelContextSwitch(KCSwitch, current_process, next_proc);

    TracePrintf(0, "ERROR: KernelExit returned unexpectedly for PID %d\n", current_process->pid);
}

int KernelWait(int *status_ptr) {
    while (1){
    PCB *child = current_process->first_child;

    // No children
    if (child == NULL) {
        TracePrintf(0, "KernelWait: no children for PID %d\n", current_process->pid);
        return ERROR;
    }

    // look for zombie children
    while (child != NULL) {
        if (child->currState == ZOMBIE) {
	    // found terminated child to reap
	    TracePrintf(1, "KernelWait: found zombie child PID %d for parent %d\n",
			child->pid, current_process->pid);

	    // copy exit status into parent memory
            if (status_ptr != NULL)
		*status_ptr = child->exit_status;

            int pid = child->pid;
	    // free child's PCB and memory
            free_proc(child);
	    // returns child's PID to parent
            return pid;
        }
        child = child->next_sibling;
    }

    // No zombie found
    TracePrintf(1, "KernelWait: no zombie found, blocking PID %d until a child exits\n",
                    current_process->pid);

    // if none are terminated, block until clock trap wakes us
    current_process->currState = BLOCKED;
    Enqueue(sleepQueue, current_process); // adds to cleep queue to mark as inactive

    PCB *next = get_next_ready_process();
    if (next == NULL) next = idle_process;

    KernelContextSwitch(KCSwitch, current_process, next);
    TracePrintf(1, "KernelWait: PID %d awakened — rechecking children\n",
                    current_process->pid);
  }
    return SUCCESS;
}

int KernelBrk(void *addr) {
     TracePrintf(1, "THIS IS OUR KERNEL FUNCTION THAT WE CREATED\n");

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
    if (new_brk < (uintptr_t)VMEM_1_BASE) return ERROR; // cannot be below region 1
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

    if (new_brk == old_brk){
	    TracePrintf(0, "Your new brk address is the same as the current!\n");
	    return 0;
    }

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

    TracePrintf(1, "THIS IS THE END OF OUR KERNEL FUNCTION THAT WE CREATED\n");
    return 0;
}

int KernelDelay(int clock_ticks) {
    TracePrintf(0, "==================================================================================\n");
    TracePrintf(1, "KernelDelay: called by PID %d with %d ticks\n", current_process->pid, clock_ticks);
    TracePrintf(1, "KernelDelay: for reference this also the num of hardware clock ticks --> %d\n", current_tick);

    if (clock_ticks < 0) {
	TracePrintf(0, "KernelDelay: invalid tick count \n");
	return ERROR;
    }

    // if 0 clock has not started
    if( clock_ticks == 0) {
	TracePrintf(1, "KernelDelay: tick count = 0, returning\n");
	return 0;
    }

    //The wake time should be ticks passed plus the current clock ticks
    current_process->wake_tick = current_tick + clock_ticks;

    // block current proccess and put it in sleep queue
    current_process->currState = BLOCKED;
    Enqueue(sleepQueue, current_process); 

    TracePrintf(1, "KernelDelay: PID %d sleeping unitil tick %lu\n", current_process->pid, current_process->wake_tick);

    // Context switch, picks next ready process, idle if none
    PCB *next = get_next_ready_process();

    if (next == NULL){
	TracePrintf(0, "KernelDelay: no other ready process, continuing with idle\n");
	next = idle_process;
    }

    next->currState = RUNNING;
    KernelContextSwitch(KCSwitch, current_process, next);
    TracePrintf(1, "KernelDelay: PID %d resumed after delay\n", current_process->pid);
    TracePrintf(0, "==================================================================================\n");
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
