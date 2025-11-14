#pragma once 
#ifndef _LOCK_H_
#define _LOCK_H_
#include "process.h"
#include "Queue.h"

#define MAX_LOCKS 64 //Idk if there is limit so just put a small amount for now

//Our lock implementation
typedef struct Lock { 

	int id;
	int in_use;
	//Keep track of the process with the current lock
	PCB *locked_process;

	//Queue of waiting processes
	Queue *lock_waiting_queue;

} Lock;

void LockTableInit(void);
Lock *get_lock(int id);
void LockFree(int id);



int KernelLockInit(int *lock_idp);
int KernelAcquire(int lock_id);
int KernelRelease(int lock_id);

#endif

