#include "syscalls.h"
#include "process.h"
#include "Queue.h"
#include "terminal.h"
#include <yalnix.h>
#include <hardware.h>
#include <ykernel.h>
#include "pipe.h"
#include "lock.h"
#include "cvar.h"

//extern globals from kernelstart.c
extern PCB *current_process;
extern Queue *blockedQueue;
extern unsigned long current_tick;
extern pte_t *kernel_page_table;
extern unsigned char *track_global;
extern Queue *readyQueue;
extern PCB *idle_process;
extern Terminal t_array[NUM_TERMINALS];
extern PCB *init_process;
extern lock_table[MAX_LOCKS];

//Helper Functions
void rollback_frames(int first_frame, int amount) {
	TracePrintf(0, "We are rolling back stack frames becauses there was an error!\n");

	for (int x = first_frame; x < amount; x++) {
		frame_free(track_global, x);
	}

	TracePrintf(0, "We are done rolling back the stack frames. Leaving! Bye!\n");
}

void data_copy(void *parent_pte, int cpfn) {
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
int KernelGetPid(void) {
    return current_process->pid;
}

int KernelFork(void) {
    TracePrintf(0, "==========================================================================================\n");
    TracePrintf(0, "This is the KernelFork() function and this is the current pid -> %d\n", current_process->pid);

    PCB *parent = current_process;
    PCB *child = spawn_proc();

    if(child == NULL) {
	    TracePrintf(0, "There was an error making a process in KernelFork()\n");
	    return ERROR;
    }
 
    //Set up info needed for fork
    unsigned int child_pid = child->pid;
    pte_t *child_reg1 = (pte_t *)child->AddressSpace;
    pte_t *parent_reg1 = (pte_t *)parent->AddressSpace;

    unsigned int frames_used = 0;
    unsigned int first_frame_used = 0;
    unsigned int used = 0;

    TracePrintf(0, "I am going to copy over parent contents to the child\n");
    //Loop through parent region 1 space and copying over to child
    for (int vpn = 0; vpn < MAX_PT_LEN; vpn++) {
	    if (!parent_reg1[vpn].valid) continue;

	    int pfn = find_frame(track_global);
	    //If there is an error free allocated frames and free the proc
	    if (pfn == ERROR) {
		    rollback_frames(first_frame_used, frames_used);
		    free_proc(child, 1);
		    return ERROR;
	    } else if(used == 0) {
		    first_frame_used = pfn;
		    used = 1;
	    }

	    frames_used++;

	    //We have the pfn, we need to cycle through each parents page table entry
	    void *parent_pte_find = (void *)((vpn + ((unsigned int )VMEM_1_BASE >> PAGESHIFT)) << PAGESHIFT);
	    TracePrintf(0, "We are going to copy the data over right now!\n");

	    data_copy(parent_pte_find, pfn);

	    //set up new page table entries for child region 1 page table
	    child_reg1[vpn].pfn = pfn;
	    child_reg1[vpn].prot = parent_reg1[vpn].prot;
	    child_reg1[vpn].valid = 1;

    }
    TracePrintf(0, "I am going to set up information for the child now\n");

    //Set up other information
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
    if (rc == ERROR) {
	    TracePrintf(0, "There was an error with KernelContextSwitch for child process on Fork()\n");
	    return ERROR;
    }

    if(current_process->pid != child_pid){
	    TracePrintf(0, "I am the parent process! Just to make sure here is my pid -> %d\n", current_process->pid);
	    TracePrintf(0, "==========================================================================================\n");
	    return child_pid;

    }

    TracePrintf(0, "I am the child process! Just to make sure here is my pid -> %d\n", current_process->pid);
    TracePrintf(0, "==========================================================================================\n");
    return 0;
}

int KernelExec(char *filename, char *argv[]) {
    TracePrintf(0, "==========================================================================================\n");
    TracePrintf(1, "Exec called by pid %d for %s\n", current_process->pid, filename);

    if (filename == NULL || argv == NULL) return ERROR;

    if(argv ==NULL){
	TracePrintf(1, "Exec: arg is NULL, nuntinouing no args\n");
    }

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
    TracePrintf(0, "==========================================================================================\n");
    return SUCCESS; // never checked
}

void KernelExit(int status) {
    TracePrintf(0, "========================================EXIT START==================================================\n");
    TracePrintf(0, "Process %d exiting with status %d\n", current_process->pid, status);
    TracePrintf(0, "This is exit syscall\n");
    TracePrintf(0,"I am going to start the exit logic!\n");
    PCB *child = current_process->first_child;	

    if (current_process->pid == 1) {
	    TracePrintf(0, "I am the parent process! I will halt now!\n");
	    Halt();
    }

    //Loop through the children process of the current process
    while (child) {
	PCB *next_child = child->next_sibling;
        child->parent = init_process;

	child->next_sibling = init_process->first_child;
	init_process->first_child = child;

        child = next_child;
    }

    current_process->first_child = NULL;

    if (current_process->parent != NULL) {
        PCB *parent = current_process->parent;
        if (parent->currState == BLOCKED) {
            TracePrintf(1, "KernelExit: waking parent PID %d\n", parent->pid);
            parent->currState = READY;
	    remove_data(blockedQueue, parent);
            Enqueue(readyQueue, parent);
        }
    }

    TracePrintf(0, "I will now erase most of my data :)\n");

    //Erase some of the data from the process
    current_process->exit_status = status;
    current_process->currState = ZOMBIE;
    TracePrintf(1, "KernelExit: process %d is now ZOMBIE, waiting to be reaped\n", current_process->pid);

    // Pick next proc to run
    PCB *next_proc = get_next_ready_process();
    if (next_proc == NULL) {
	    TracePrintf(0, "We have not other process that can run so run idle :)\n");
	    next_proc = idle_process;
    }

    // context switch from dying proc
    TracePrintf(1, "KernelExit: switching from PID %d to PID %d\n", current_process->pid, next_proc->pid);
    int rc = KernelContextSwitch(KCSwitch, current_process, next_proc);
    if (rc == ERROR) {
	    TracePrintf(0, "ERROR: KernelExit returned unexpectedly for PID %d\n", current_process->pid);
	    return;
    }

    TracePrintf(0, "===========================================EXIT END===============================================\n");
}

int KernelWait(int *status_ptr) {
    TracePrintf(0, "=============================================================================>\n");
    TracePrintf(1, "This is wait syscall\n");

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
	    TracePrintf(1, "KernelWait: found zombie child PID %d for parent %d\n", child->pid, current_process->pid);

	    // copy exit status into parent memory
            if (status_ptr != NULL)
		*status_ptr = child->exit_status;

            int pid = child->pid;
	    // free child's PCB and memory
            free_proc(child, 1);
	    // returns child's PID to parent
            return pid;
        }
        child = child->next_sibling;
    }

    // No zombie found
    TracePrintf(1, "KernelWait: no zombie found, blocking PID %d until a child exits\n", current_process->pid);

    // if none are terminated, block until clock trap wakes us
    current_process->currState = BLOCKED;
    Enqueue(blockedQueue, current_process); // adds to cleep queue to mark as inactive

    PCB *next = get_next_ready_process();
    if (next == NULL) next = idle_process;

    KernelContextSwitch(KCSwitch, current_process, next);
    TracePrintf(1, "KernelWait: PID %d awakened — rechecking children\n", current_process->pid);
    TracePrintf(0, "===========================================WAIT END==========================>\n");
    return SUCCESS;
    }
}

int KernelBrk(void *addr) {
    TracePrintf(0, "=============================================================================>\n");
     TracePrintf(1, "THIS IS OUR KERNEL FUNCTION THAT WE CREATED\n");

     TracePrintf(1, "KernelBrk: requested addr=%p\n", addr);
     addr = (void *)UP_TO_PAGE(addr);

    // if addr < what the brk was when the program started,
    // or addr is in the stack mappings, fail. (checking for null not sufficient

    void *heap_start = current_process->user_heap_brk;
    void *stack_start = current_process->user_stack_ptr;

    TracePrintf(0, "current heap_start = %p\n", heap_start);
    TracePrintf(0, "current stack_start = %p\n", stack_start);
    TracePrintf(0, "current rounded addr = %p\n", addr);

    if (addr < heap_start) {
        TracePrintf(0, "KernelBrk: Requested addr (%p) is below heapstart (%p)\n", addr, heap_start);
        return ERROR;
    }

    if (addr >= stack_start - PAGESIZE) {
        TracePrintf(0, "KernelBrk: Requested addr (%p) collides with stack (%p)\n", addr, stack_start);
        return ERROR;
    }

    if (!addr) return ERROR;

    // Converting to integers
    uintptr_t new_brk = (uintptr_t)addr; // new break
    uintptr_t old_brk = (uintptr_t)current_process->user_heap_brk; //currrent brk

    pte_t *pt = current_process->AddressSpace;

    // computing stack boundary
    uintptr_t stack_base_vpn = ((uintptr_t)current_process->user_stack_ptr) >> PAGESHIFT;

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

    if (new_brk == old_brk) {
	    TracePrintf(0, "Your new brk address is the same as the current!\n");
	    return 0;
    }

    // Growing and shinking heap
    if (new_brk > old_brk) {
    // grow heap
	int start_vpn = old_vpn; 
	int end_vpn = new_vpn - 1;

	TracePrintf(1, "KernelBrk: grow heap from vpn %d to %d\n", start_vpn, end_vpn);

	for (int vpn = start_vpn; vpn<= end_vpn; vpn++){
		// we skip pages already valid
		if (pt[vpn].valid) continue;

		int pfn = find_frame(track_global);
		if (pfn == ERROR){
			TracePrintf(0, "KernelBrk: OOM while growing at vpn=%d\n", vpn);
			// roll page pages we just mapped
			for (int r = start_vpn; r < vpn; r++) {
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
	} else {

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
    current_process->user_heap_brk = (void *)new_brk;
    TracePrintf(1, "KernelBrk: success, new brk=%p\n", current_process->user_heap_brk);

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
    if (clock_ticks == 0) {
	TracePrintf(1, "KernelDelay: tick count = 0, returning\n");
	return 0;
    }

    //The wake time should be ticks passed plus the current clock ticks
    current_process->wake_tick = current_tick + clock_ticks;

    // block current proccess and put it in blocked queue
    current_process->currState = BLOCKED;
    Enqueue(blockedQueue, current_process); 

    TracePrintf(1, "KernelDelay: PID %d blocked unitil tick %lu\n", current_process->pid, current_process->wake_tick);

    // Context switch, picks next ready process, idle if none
    PCB *next = get_next_ready_process();
    if (next == NULL) {
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
	//Check if inputs are valid
	if (tty_id < 0 || tty_id >= NUM_TERMINALS || buf == NULL || len < 0){
		TracePrintf(0, "TtyRead: One of your arguments was invalid!\n");
		return ERROR;
	}

	if (t_array[tty_id].read_waiting_process == NULL) {
		TracePrintf(0, "TtyRead: I have no waiting process!\n");
	}

	if (t_array[tty_id].input_read_head == NULL) { 
		TracePrintf(0, "TtyRead: My input head is also null so i must wait!\n");
	}

	//Check if another caller is blocked or there is no input; if yes block this caller 
	while (t_array[tty_id].read_waiting_process != NULL || t_array[tty_id].input_read_head == NULL) { 
		//If proc already in blocked queue do not keep adding it back in
		if(!in_queue(blockedQueue, (void *)current_process)) {
			TracePrintf(0, " TtyRead: I will now put process %d into the blocked Queue! This should happen once!\n");
			current_process->currState = BLOCKED;
			Enqueue(blockedQueue, current_process);
			}

		//Get a new process
		PCB *next = get_next_ready_process();
		TracePrintf(0, "TtyRead: I put process %d into the blocked queue and im going to switch into %d\n", current_process->pid, next->pid);

		if (KernelContextSwitch(KCSwitch, current_process, next) < 0) {
			TracePrintf(0, "TtyRead:There was an error with the KCSwitch in TtyRead!\n");
			Halt(); //for testing
			return ERROR;
		}
	}

	TracePrintf(0, "==> We got a message from a user terminal!\n");
	MessageNode *curr_message = t_array[tty_id].input_read_head;
	
	//Case: The current message node has sufficent bytes for us to read :)
	if (curr_message->length >= len) {
		if (curr_message->length == len) {
			TracePrintf(0, "TtyRead: Hey! We can return to you a message of the exact length that you requested!\n");

			//Copy into buffer and pop off message list
			memcpy(buf, (void *)curr_message->message, len);	
			MessageNode *head = read_remove_message(tty_id);
			if (head == NULL) {
				TracePrintf(0, "TtyRead: removing message node didn't go well! Returning ERROR!\n");
				return ERROR;
			}

			//free message buffer and Node
			free(head->message);
			free(head);	
			return len;
		}

		//memcpy data, shift bytes over, and realloc the buffer to smaller size
		TracePrintf(0, "TtyRead: Hey! The message I found is longer than you requested! I will return only what you requested!\n");
		memcpy(buf, (void *)curr_message->message, len);
		memmove(curr_message->message, curr_message->message + len, curr_message->length - len);
		curr_message->length -= len;
		curr_message->message = realloc(curr_message->message, curr_message->length);
		return len;
	}

	//Case: The requested len > curr_message->length but we only have 1 node
	//Thus we just return what we have at this moment
	if (curr_message->next == NULL) { 
		TracePrintf(0, "TtyRead: Hey! I can give you your fully requested amount :( but I can give you what i have right now!\n");
		memcpy(buf, (void *)curr_message->message, curr_message->length);
		int message_length = curr_message->length;
		free(curr_message->message);
		free(curr_message);
		t_array[tty_id].input_read_head = NULL;
		TracePrintf(0, "TtyRead: Freed message node, message buffer, and set input head to NULL! Bye :)\n");
		return message_length;
	}

	//Case: We need to string multiple message nodes together
	//We loop until we can satisfy len or until we reach then end
	//Note: We dont have to stricly return len only as much as we currently have
	
	unsigned int bytes_copied = 0;
	while (bytes_copied < len && curr_message->next != NULL) {
		int bytes_in_node = curr_message->length; //Bytes in the curernt node
		int bytes_search = len - bytes_copied; //Bytes we need left to satisfy user request
		int bytes_to_copy = (bytes_in_node < bytes_search) ? bytes_in_node : bytes_search;

		//copy to buffer; start from + bytes_copied to not override data
		memcpy(buf + bytes_copied, curr_message->message, bytes_to_copy);
		bytes_copied += bytes_to_copy;

		if (bytes_to_copy == curr_message->length) {
			TracePrintf(0, "TtyRead: We have read all the data from this message node! Moving to the next :)\n");

			curr_message = curr_message->next;
			MessageNode *head = read_remove_message(tty_id);

			//Clean up message node and message buffer
			free(head->message);
			free(head);

		 } else {
			//Incase we reach a node and we only use part of the its buffer
			TracePrintf(0, "TtyRead: Great we only used part of this node! We shift it around to save for the next call!\n");
			memmove(curr_message->message, curr_message->message + bytes_to_copy, curr_message->length - bytes_to_copy);
			curr_message->length -= bytes_to_copy;
			realloc(curr_message->message, curr_message->length);
		 }

	}

	return bytes_copied;
}


int KernelTtyWrite(int tty_id, void *buf, int len) {
	if (tty_id < 0 || tty_id >= NUM_TERMINALS || buf == NULL || len < 0) {
		TracePrintf(0, "One of your arguments was invalid!\n");
		return ERROR;
	}

	if(t_array[tty_id].transmit_waiting_process != NULL) {
		TracePrintf(0, "Sorry but another process is already writing to this terminal! PLEASE WAIT!\n");
		return ERROR;
	}

	//TODO: Check if the buffer is valid {This should be a function we use around}
	
	//Create buffer in region 0
	char *kbuffer = calloc(1, len);
	if (kbuffer == NULL) {
		TracePrintf(0, "Error with malloc for TtyWrite\n");
		return ERROR;
	}

	//Copy user buffer -> kernel buffer
	memcpy(kbuffer, buf, len);

	//If we only need to transmit once
	if (len < TERMINAL_MAX_LINE) {
		TracePrintf(0, "TtyWrite: Nice! Your message can be sent in one trip!\n");
		TracePrintf(0, "Process %d has sent a message! I will block this process now until we can run again!\n", current_process->pid);	

		//Put the current process in the blocked queue
		t_array[tty_id].transmit_waiting_process = current_process;
		current_process->currState = BLOCKED;

		//To avoid being wasteful and creating a MessageNode {FREE LATER IN TRAP}
		t_array[tty_id].one_message_buffer = kbuffer; 
		t_array[tty_id].one_message_length = len;

		Enqueue(blockedQueue, (void *)current_process); //Add to block queue
		
		TtyTransmit(tty_id, kbuffer, len);

		PCB *next = get_next_ready_process();
		if (KernelContextSwitch(KCSwitch, current_process, next) < 0) {
			TracePrintf(0, "There was an error with the KCSwitch in TtyWrite!\n");
			Halt(); //for testing
			return ERROR;
		}

		TracePrintf(0, "Transmit was done and returing to you this much -> %d", len);
		return len;
	}

	TracePrintf(0, "Looks like we are going to transmit your message in chunks!\n");
	TracePrintf(0, "For debug purposes this is the amount that len is -> %d\n", len);

	//--------------------------------------------------- Support logic for when len > TERMINAL_MAX_LINE
	
	//We need to split by chunks of TERMINAL_MAX_LINE - 1
	// -1 to leave space for null terminator
	unsigned int bytes_left = len;
	unsigned int buffer_size = TERMINAL_MAX_LINE - 1;
	unsigned int chunks = 0;

	for (unsigned int i = 0; i < len; i += buffer_size) {
		//For each loop create a node, add message to list, repeat
		unsigned int chunk_split = (bytes_left < buffer_size) ? bytes_left : buffer_size;
		
		//Create the node sturct 
		MessageNode *message_node = calloc(1, sizeof(MessageNode));

		//Calloc buffer for message_node, assign to field in struct
		char* mbuffer = calloc(1, TERMINAL_MAX_LINE);
		if (message_node == NULL || mbuffer == NULL) {
			TracePrintf(0, "TtyWrite: There was an error with creating a MessageNode!\n");
			return ERROR;
		}

		message_node->message = mbuffer;
		mbuffer[TERMINAL_MAX_LINE] = '\0'; //Might not need this???
						   
		memcpy(message_node->message, kbuffer + i, chunk_split);
		message_node->length = chunk_split;

		//Add the messages to the linked list
		add_message(tty_id, message_node);
		bytes_left -= chunk_split;
		chunks++;

	}

	//-------------------------------- At this point we just have to transverse the node list and call transmit
	TracePrintf(0, "We have made all messages nodes! there is this many chunks --> %d\n", chunks);
	free(kbuffer);

	if(t_array[tty_id].transmit_message_head == NULL){
		TracePrintf(0 , "THIS SHOULD NOT HAPPEN! Please check the logic for setting up the linked list of messages!");
		Halt(); //Test
		return ERROR;
	}

	//Block the process since we need more than 1 transmit
	t_array[tty_id].transmit_waiting_process = current_process;
	current_process->currState = BLOCKED;
	Enqueue(blockedQueue, (void *)current_process); //Add to block queue
						      
	MessageNode *current = t_array[tty_id].transmit_message_head;
	TracePrintf(0, "We are going to transmit a message!\n");
	TtyTransmit(tty_id, (void *)current->message, current->length);

	PCB *next = get_next_ready_process();
	if (KernelContextSwitch(KCSwitch, current_process, next) < 0) {
			TracePrintf(0, "There was an error with the KCSwitch in TtyWrite!\n");
			Halt(); //For testing
			return ERROR;
	}

	//The MessageNode should be freed in the trap handler {I think-> It should be and also its message field}
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
    TracePrintf(0, "PipeInit() called by PID %d\n", current_process->pid);

    // vailidating pointer
    if(pipe_id_ptr == NULL) {
        TracePrintf(0, "PipeInt: invalid user pointer\n");
        return ERROR;
    }

    // find free slot in the global pipe table
    for (int i = 0; i < MAX_PIPES; i++){
        // checking if slot is occupied
        if (!pipe_table[i].in_use) {
            Pipe *p = &pipe_table[i];

            // initilizing pipe state
            memset(p->buffer, 0, PIPE_BUFFER_LEN);
            p->head = p->tail = p->count = 0;
            p->in_use = 1;
            p->waiting_readers = initializeQueue();
            p->waiting_writers = initializeQueue();

            // writing ID back to user memory
            *pipe_id_ptr = i;

            TracePrintf(0, "PipeInit: created pipe ID %d\n", i);
            return 0;
        }
    }

    TracePrintf(0, "PipeInit: no free pipes avaliable!\n");
    return ERROR;
}

int KernelPipeRead(int pipe_id, void *buf, int len) {
    TracePrintf(0, "PipeRead() called by PID %d for pipe %d, len=%d\n",
		current_process->pid, pipe_id, len);

    // validating input
    if (buf == NULL || len <= 0) {
	TracePrintf(0,"PipeRead: invalid args\n");
 	return ERROR;
    }

    Pipe *p = get_pipe(pipe_id);
    if (p == NULL || !p->in_use) {
        TracePrintf(0, "PipeRead: invalid pipe ID %d\n", pipe_id);
	return ERROR;
    }

    int available = bytes_available(p);
    if (available <= 0) {
	TracePrintf(0, "PipeRead: pipe empty, blocking PID %d\n", current_process->pid);

        // blocking current process
        current_process->currState = BLOCKED;
        Enqueue(p->waiting_readers, current_process);

	//switching context to the next ready process
        PCB *next = get_next_ready_process();
        if (next == NULL) next = idle_process;
        KernelContextSwitch(KCSwitch, current_process, next);

        available = bytes_available(p);
    }

    //reading from pipe up to len
    int to_read = (len > available) ? available : len;
    int read = PipeReadBytes(p, buf, to_read);
    TracePrintf(0, "PipeRead: read %d bytes from pipe %d\n", read, pipe_id);


    // wake waiting waiter
    if (p->waiting_writers && p->waiting_writers->head != NULL) {
        PCB *writer = Dequeue(p->waiting_writers);
        writer->currState = READY;
        Enqueue(readyQueue, writer);
        TracePrintf(0, "PipeRead: woke writer PID %d\n", writer->pid);
    }

    // bytes read
    return read;
}

int KernelPipeWrite(int pipe_id, void *buf, int len) {
    TracePrintf(0, "PipeWrite() called by PID %d for the pipe %d , len=%d\n",
            current_process->pid, pipe_id, len);

    // validating input
    if (buf == NULL || len <= 0) {
        TracePrintf(0, "PipeWrite: invalid args\n");
        return ERROR;
    }

    Pipe *p = get_pipe(pipe_id);
    if (p == NULL || !p->in_use) {
        TracePrintf(0, "PipeWrite: invalid pipe ID %d\n", pipe_id);
        return ERROR;
    }

    // checking how much space is left
    int available = space_available(p);
    if (available <= 0) {
        TracePrintf(0, "PipeWrite: pipe full, blocking PID %d\n", current_process->pid);

        // blocking
        current_process->currState = BLOCKED;
        Enqueue(p->waiting_writers, current_process);

        PCB *next = get_next_ready_process();
        if (next == NULL) next = idle_process;
        KernelContextSwitch(KCSwitch, current_process, next);

        available = space_available(p);
    }

    // copying data into pipe
    int written = PipeWriteBytes(p, buf, len);
    TracePrintf(0, "PipeWrite: wrote %d bytes to pipe %d\n", written, pipe_id);

    // wake waiting reader
    if (p->waiting_readers && p->waiting_readers->head != NULL) {
        PCB *reader = Dequeue(p->waiting_readers);
        reader->currState = READY;
        Enqueue(readyQueue, reader);
        TracePrintf(0, "PipeWrite: woke reader PID %d\n", reader->pid);
    }

    // how many btyes we successfully wrote
    return written;
}

/* ============================
 * Synchronization Syscalls
 * ============================
 */


int KernelLockInit(int *lock_idp) {
        TracePrintf(0, "LockInit() called PID %d\n", current_process->pid);

        // validating pointer
        if (lock_idp == NULL) {
                TracePrintf(0, "KernelLockInit: The pointer that you gave me was NULL\n");
                return ERROR;
        }

        //searching lock table for free slot
        for (int i = 0; i < MAX_LOCKS; i++) {
            Lock *lk = &lock_table[i];

            if (!lk->in_use) {
                // initialize lock entry
                lk->id = i;
                lk->in_use = 1;
                lk->locked_process = NULL;
                lk->lock_waiting_queue = initializeQueue();

                // writing id back to user memory
                *lock_idp = i;

                TracePrintf(0, "LockInit(): created lock ID %d\n", i);
                return 0;
            }
        }
        TracePrintf(0, " LockInit: no free lock slots available!\n");
        return ERROR;
}

int KernelAcquire(int lock_id) {
    TracePrintf(0, "Acquire() called by PID %d for lock %d\n",
                current_process->pid, lock_id);

    Lock *lk = get_lock(lock_id);
    if (lk == NULL || !lk->in_use) {
        TracePrintf(0, "Acquire: invalid lock ID %d\n", lock_id);
        return ERROR;
    }

    // if lock is free
    if (lk->locked_process == NULL) {
        lk->locked_process = current_process;
        TracePrintf(0, " Acquire: PID %d acuired lock %d\n",
                    current_process->pid, lock_id);
        return 0;
    }

    // if lock is already held by this process
    if (lk->locked_process == current_process) {
        TracePrintf(0, "Acquire: PID %d attempted to re-lock lock %d\n",
                    current_process->pid, lock_id);
        return ERROR;
    }

    //lock is held by someone else, blocking
    //if (lk->locked_process == current_process){
    TracePrintf(0, "Acquire: PID %d blocking on lock %d\n",
                current_process->pid, lock_id);

    current_process->currState = BLOCKED;
    Enqueue(lk->lock_waiting_queue, current_process);

    PCB *next = get_next_ready_process();
    if (next == NULL) next = idle_process;

    KernelContextSwitch(KCSwitch, current_process, next);

    lk->locked_process = current_process;

    TracePrintf(0, "Acquire: PID %d woke and aquired lock %d\n",
                current_process->pid, lock_id);

    return 0;
}

int KernelRelease(int lock_id) {

    TracePrintf(0," Realease() called by PID %d for lock %d\n",
                current_process->pid, lock_id);

    Lock *lk = get_lock(lock_id);
    if (lk == NULL || !lk->in_use) {
        TracePrintf(0, "Release: invalid lock ID %d\n", lock_id);
        return ERROR;
    }

    // only the owner can release the lock
    if (lk->locked_process != current_process) {
        TracePrintf(0,
                    "Release: PID %d does not own lock %d (owner PID %d)\n",
                    current_process->pid,
                    lock_id,
                    (lk->locked_process ? lk->locked_process->pid : -1));
        return ERROR;
    }


    //if no one is waiting, freeing lock
    if (lk->lock_waiting_queue == NULL || lk->lock_waiting_queue->head == NULL) {
        TracePrintf(0, "Release: no waiters, freeing lock %d\n", lock_id);
        lk->locked_process = NULL;
        return 0;
    }

    PCB *next_owner = Dequeue(lk->lock_waiting_queue);
    //making the waiter runable and giving it the lock
    next_owner->currState = READY;

    Enqueue(readyQueue, next_owner);
    lk->locked_process = next_owner;

    TracePrintf(0, "Release: PID %d handed lock %d to PID %d\n",
                current_process->pid, lock_id, next_owner->pid);

    return 0;
}

int KernelCvarInit(int *cvar_idp) {
    TracePrintf(0, "CvarInit() called by PID %d\n", current_process->pid);

    //validating pointer 
    if (cvar_idp == NULL) {
    TracePrintf(0, "CvarInit: invalid user pointer\n");
    return ERROR;
    }

    // allocating new Cvar structure
    Cvar *cv = calloc(1, sizeof(Cvar));
    if (cv == NULL) {
        TracePrintf(0, "CvarInit: malloc failed\n");
        return ERROR;
    }

    // assign ID and initialize fields
    cv->id = cvar_count++;
    cv->in_use = 1;
    cv->waiters = initializeQueue();
    cv->next = NULL;

    // adding to global list 
    add_cvar(cv);

    //write the ID back to user memory
    *cvar_idp = cv->id;

    TracePrintf(0, "CvarInit: created Cvar ID %d\n", cv->id);
    return 0;

}

int KernelCvarSignal(int cvar_id) {
    TracePrintf(0, "CvarSignal() called by PID %d for cvar %d\n",
                current_process->pid, cvar_id);

    // validating
    Cvar *cv = find_cvar(cvar_id);
    if (cv == NULL || !cv->in_use) {
        TracePrintf(0,"CvarSignal: invalid cvar ID %d\n", cvar_id);
        return ERROR;
    }

    // cheking if anyone is waiting 
    if (cv->waiters == NULL || cv->waiters->head == NULL) {
        TracePrintf(0, "CvarSignal: no process waiting on CV %d\n", cvar_id);
        return 0;
    }

    // Dequeue one waiter
    CvarWaiter *w = Dequeue(cv->waiters);
    if (w == NULL) {
        TracePrintf(0, "CvarSignal: dequeue returned null/n");
        return 0;
    }

    PCB *proc = w->proc;
    int lock_id = w->lock_id;
    free(w);

    // marking the proc as ready 
    proc->currState = READY;
    Enqueue(readyQueue, proc);
    TracePrintf(0, "CvarSignal: woke PID %d waiting on CV %d (will reacquire lock %d)\n",
                proc->pid, cvar_id, lock_id);

    return 0;
}

int KernelCvarBroadcast(int cvar_id) {
    TracePrintf(0, "CvarBroadcast() called by PID %d for cvar %d\n",
                current_process->pid, cvar_id);

    // validate
    Cvar *cv = find_cvar(cvar_id);
    if (cv == NULL || !cv->in_use) {
        TracePrintf(0, "CvarBroadcast: invalid cvar ID %d\n", cvar_id);
        return ERROR;
    }

    //check if proc waiting
    if (cv->waiters == NULL || cv->waiters->head == NULL) {
        TracePrintf(0, "CvarBroadcast: no processes waiting on CV %d\n", cvar_id);
        return 0;
    }

    // wake every proc in waiting queue
    while (cv->waiters->head != NULL) {
        CvarWaiter *w = Dequeue(cv->waiters);
        if (w == NULL) break;
        PCB *proc = w->proc;
        int lock_id = w->lock_id;
        free(w);

        proc->currState = READY;
        Enqueue(readyQueue, proc);
        TracePrintf(0, "CvarBroadcast: woke PID %d (CV %d, lock %d)\n",
                    proc->pid, cvar_id, lock_id);
    }

    TracePrintf(0,"CvarBroadcast: done waking all waiters on CV %d\n", cvar_id);
    return 0;
}

int KernelCvarWait(int cvar_id, int lock_id) {
    TracePrintf(0, "CvarWait() called by PID %d (cvar %d, lock %d)\n",
                current_process->pid, cvar_id, lock_id);

    // validate
    Cvar *cv = find_cvar(cvar_id);
    Lock *lk = get_lock(lock_id);
    if (cv == NULL || lk == NULL) {
        TracePrintf(0, "CvarWait: invalid cvar or lock ID\n");
        return ERROR;
    }

    TracePrintf(0, "CvarWait: releasing lock %d before sleeping\n", lock_id);
    KernelRelease(lock_id);

    // blocking current proc on cvar's waiting queue
    CvarWaiter *w = calloc(1, sizeof(CvarWaiter));
    w->proc = current_process;
    w->lock_id = lock_id;
    Enqueue(cv->waiters, w);

    current_process->currState = BLOCKED;
    TracePrintf(0, "CvarWait: PID %d blocked on CV %d\n", current_process->pid, cvar_id);

    PCB *next = get_next_ready_process();
    if (next == NULL) next = idle_process;
    KernelContextSwitch(KCSwitch, current_process, next);

    //when process is woken it must re-aquire the lock before retunring 
    TracePrintf(0, "CvarWait: PID %d woke up, re-acquiring lock %d\n", current_process->pid, lock_id);
    KernelAcquire(lock_id);

    return 0;
}

int KernelReclaim(int id) {
    TracePrintf(0, "Reclaim() called by PID %d for ID %d\n", 
                current_process->pid, id);

    // finding and freeing lock
    Lock *lk = get_lock(id);
    if (lk && lk->in_use) {
        TracePrintf(0, "Reclaim freeing Lock ID %d\n", id);

        if (lk->lock_waiting_queue)
            free(lk->lock_waiting_queue);

        lk->lock_waiting_queue = NULL;
        lk->locked_process = NULL;
        lk->in_use = 0;

        return 0;
    }

    // finding and freeing condition variable
    Cvar *cv = find_cvar(id);
    if (cv && cv->in_use) {
        TracePrintf(0, "Reclaim: freeing Cvar ID %d\n", id);

        if (cv->waiters)
            free(cv->waiters);

        cv->waiters = NULL;
        cv->in_use = 0;

        return 0;
    }

    // finding and freeing pipe
    Pipe *p = get_pipe(id);
    if (p && p->in_use) {
        TracePrintf(0, "Reclaim: free Pipe ID %d\n", id);
        PipeFree(id);
        return 0;
    }

    TracePrintf(0, "Reclaim: invalid ID  %d not found\n", id);
    return ERROR;
}
