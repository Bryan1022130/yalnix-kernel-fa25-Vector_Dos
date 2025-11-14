#include <yalnix.h>
#include <ylib.h>
#include "lock.h"
#include "Queue.h"
#include "process.h"


Lock lock_table[MAX_LOCKS];

extern PCB *current_process;
extern PCB *idle_process;
extern Queue *readyQueue;

//int lock_count = 0;
//Lock *linked_locks = NULL; //maybe just make them all free ?


void LockTableInit(void) {
    memset(lock_table, 0, sizeof(lock_table));

    for(int i = 0; i < MAX_LOCKS; i++) {
        lock_table[i].id = i;
        lock_table[i].in_use = 0;
        lock_table[i].locked_process = NULL;
        lock_table[i].lock_waiting_queue = NULL;
    }

}


Lock *get_lock(int id) {
    if (id < 0 || id >= MAX_LOCKS)
        return NULL;

    if (!lock_table[id].in_use)
        return NULL;

    return &lock_table[id];
}


void LockFree(int id) {
    if (id < 0 || id >= MAX_LOCKS)
        return;

    Lock *lk = &lock_table[id];

    lk->in_use = 0;
    lk->locked_process = NULL;

    if (lk->lock_waiting_queue) {
        free(lk->lock_waiting_queue);
        lk->lock_waiting_queue = NULL;
    }
}


/*
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
*/

//WHEN I AM DONE MOVE THESE TO SYSCALLS.c
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

/*
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
*/
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
/*
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
*/
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
/*	if (lock_id < 0){
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
*/

}
