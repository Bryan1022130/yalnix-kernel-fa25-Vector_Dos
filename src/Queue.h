#pragma once
#include <stdint.h>
#include <stddef.h>
#include <ykernel.h>
#include <hardware.h> // For macros regarding kernel space
#include <yalnix.h>
#include <ylib.h>
#include <yuser.h>
#include <sys/mman.h>


//Node struct that represents a single element in the Queue
typedef struct QueueNode{ 
	void *data;
	struct QueueNode *next;
}QueueNode;

//Struct to represent the entire Queue
//Keep track of the Head, Tail, and total size 
typedef struct{ 
	QueueNode *head;
	QueueNode *tail;
	int size;
}Queue;

//Get a clean and freshly made Queue struct 
Queue *initalizeQueue(void);

// Get the total size of the queue currently
int getSize(Queue *MQueue);

//Add a node to the start of the queue
void Enqueue(Queue *MQueue, void *data);

//Take a node off the head of the queue since it is a FIFO data struct 
void* Dequeue(Queue *MQueue);

//Check if the Queue is currently empty
int isEmpty(Queue *MQueue);

// Peek at the head Node
QueueNode *peek(Queue *MQueue);

