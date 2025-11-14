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
