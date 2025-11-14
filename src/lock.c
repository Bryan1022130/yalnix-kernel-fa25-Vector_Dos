#include <yalnix.h>
#include <ylib.h>
#include "lock.h"
#include "Queue.h"
#include "process.h"

Lock lock_table[MAX_LOCKS];

extern PCB *current_process;
extern PCB *idle_process;
extern Queue *readyQueue;

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
