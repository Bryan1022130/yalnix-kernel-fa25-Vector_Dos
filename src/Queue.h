#pragma once
#include <stdint.h>
#include <stddef.h>
#include <ykernel.h>
#include <hardware.h> 
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
typedef struct{ 
	QueueNode *head;
	QueueNode *tail;
	int size;
}Queue;

//Get a clean and freshly made Queue struct 
Queue *initializeQueue(void);

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

//To remove a specific Node
void remove_data(Queue *MQueue, void *find_data);

//Check if a proc is in the queue
int in_queue(Queue *MQueue, void *find_data);
