#pragma once
#include "Queue.h"
#include "process.h"

typedef struct CvarWaiter {
    PCB *proc; // the proc blocked on this condition
    int lock_id; // the lock it must reaquire later
} CvarWaiter;

typedef struct Cvar {
    int id;
    int in_use; // 1 if allocated 0 if free
    Queue *waiters;
    struct Cvar *next;
} Cvar;

// global list head 
extern Cvar *linked_cvars;

// ID counter
extern int cvar_count;

void CvarTableInit(void);
Cvar *find_cvar(int id);
void add_cvar(Cvar *cv);

