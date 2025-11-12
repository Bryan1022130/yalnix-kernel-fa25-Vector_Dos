#include <yalnix.h>
#include <ylib.h>
#include "lock.h"
#include "Queue.h"
#include "process.h"

extern PCB *current_process;
int lock_count = 0;
extern PCB *current_process;
Lock *linked_locks = NULL; //maybe just make them all free ?


void add_lock(Lock *locky) {
	if (linked_locks ==  NULL) { 
		TracePrintf(0, "Hey! We are going to make lock %d the head node!\n", locky->id);
		linked_locks = locky;
		return;
	}

	TracePrintf(0, "Adding lock %d to the end of the list!", locky->id);
	
	//Transver the linke list and attach the lock to the end
	Lock *current = linked_locks;
	while (current->next != NULL) { 
		current = current->next;
	}

	TracePrintf(0, "Adding the lock now!\n");
	current->next = locky;
	return;
}

Lock *find_lock(int id) {
	Lock *current = linked_locks;
	Lock *prev = NULL;

	if (current == NULL) { 
		TracePrintf(0, "The lock list is currently empty! Error possibly!\n");
		return NULL;
	}

	//If lock we need as the head node
	if(current->id == id) { 
		TracePrintf(0, "Great we found the lock at the head!\n");
		if (current->next != NULL) { 
			TracePrintf(0, "Updating the head of the linked list!\n");
			linked_locks = current->next;
			current->next =  NULL;
			return current;
		}  

		TracePrintf(0, "Removing the head! linked_locks will not be empty!\n");
		linked_locks = NULL;
		return current;	
	}
	
	//Transverse the linked list
	while (current != NULL){
		if (current->id == id) { 
			TracePrintf(0, "Hey! We found lock %d! We will now return!\n");	
			//remove from linked list and update prev
			prev->next = current->next;
			current->next = NULL;
			return current;
		}

		prev = current;
		current = current->next; 
	}

	TracePrintf(0, "I did not find your lock! Your input -> %d was invalid!\n", id);
	return NULL;
}

//WHEN I AM DONE MOVE THESE TO SYSCALLS.c
int KernelLockInit(int *lock_idp) {
	if (lock_idp == NULL) { 
		TracePrintf(0, "KernelLockInit: The pointer that you gave me was NULL\n");
		return ERROR;
	}

	//First allocate a new Lock
	Lock *lock_init = calloc(1, sizeof(Lock));
	if (lock_init == NULL) { 
		TracePrintf(0, "KernelLockInit: There was an error with creating a new lock!\n");
		return ERROR;
	}

	TracePrintf(0, "KernelLockInit: We have created a new Lock! Time to set up its information :)\n");

	lock_init->current_state = FREE_LOCK; //Set as free
	lock_init->locked_process = NULL; //No process has the lock currently
	lock_init->next = NULL;
					  
	Queue *spawn_queue = initializeQueue(); 
	if (spawn_queue == NULL) { 
		TracePrintf(0, "KernelLockInit: The Queue tried to spawn failed!\n");
		return ERROR;
	}

	lock_init->lock_waiting_queue = spawn_queue; //Set up the queue
	lock_idp = &lock_count; //save identifier at lock_idp
	lock_init->id = lock_count; //Save its id 
	lock_count++;//increase for next lock create
	
	add_lock(lock_init); //Add to linked list for searching
	return SUCCESS;
}

int KernelAcquire(int lock_id) { 
	if (lock_id < 0){
		TracePrintf(0, "KernelAcquire: Hey! You can give me a negative number!\n");
		return ERROR;
	}

	Lock *lock = find_lock(lock_id);
	if (lock == NULL) { 
		TracePrintf(0, "KernelAcquire: We could not find the lock with the if of -> %d\n", lock_id);
		return ERROR;
	}

	//Not its time to make the lock ours
	if (lock->locked_process == NULL && lock->current_state == FREE_LOCK) {
		//First check if there is a process waiting and to let it acquire the lock first
		if (!isEmpty(lock->lock_waiting_queue)) {
			TracePrintf(0, "Hey lets be fair here! The waiting process will go first!\n");
			//Get process and store lock info
			PCB* pop_proc = Dequeue(lock->lock_waiting_queue);
			lock->locked_process = pop_proc;
			lock->current_state = LOCKED;
			add_lock(lock);
			TracePrintf(0, "The process with pid -> %d now had the lock and current process -> %d will wait", pop_proc->pid, current_process->pid);
			//Block the current process
			Enqueue(lock->lock_waiting_queue, current_process);
			return SUCCESS;
		} 

		TracePrintf(0, "KenrnelAcquire: Hey! The lock is free! Now locking!\n");
		//update lock information
		lock->locked_process = current_process;
		lock->current_state = LOCKED;
		add_lock(lock);
		//Maybe i wont add it back because its not free { Not sure yet}
		return SUCCESS;
	}

	TracePrintf(0, "Okay! This lock is currently being used but you will be put into the queue!\n");
	Enqueue(lock->lock_waiting_queue, current_process);
	TracePrintf(0, "I will return error but you will be on the queue!\n");
	return ERROR;
}

int KernelRelease(int lock_id) { 
	if (lock_id < 0){
		TracePrintf(0, "KernelAcquire: Hey! You can give me a negative number!\n");
		return ERROR;
	}

	Lock *lock = find_lock(lock_id);
	if (lock == NULL) { 
		TracePrintf(0, "KernelAcquire: We could not find the lock with the if of -> %d\n", lock_id);
		return ERROR;
	}

	if (lock->locked_process == current_process && lock->current_state == LOCKED) {
		TracePrintf(0, "KenrnelAcquire: Hey! The lock is free! Now locking!\n");
		//update lock information
		lock->locked_process = NULL;
		lock->current_state = FREE_LOCK;
		add_lock(lock);
		return SUCCESS;
	}

	TracePrintf(0, "You do not have the power to release this lock! It belongs to process -> %d ERORR!\n", lock->locked_process->pid);
	return ERROR;
}
