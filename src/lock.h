#pragma once 
#include "process.h"
#include "Queue.h"

#define MAX_LOCKS 64 //Idk if there is limit so just put a small amount for now

typedef enum{
	FREE_LOCK,
	LOCKED,
	ERROR_LOCK
}LockState;

//Our lock implementation
typedef struct lock { 
	//Keep track of the process with the current lock
	PCB *locked_process;

	//Queue of waiting processes
	Queue *lock_waiting_queue;

	//Keep a state of the lock
	LockState current_state;

	//id of the lock
	int id;
	
	//Pointer to next lock 
	struct lock *next;
} Lock;

int KernelLockInit(int *lock_idp);
int KernelAcquire(int lock_id);
int KernelRelease(int lock_id);
