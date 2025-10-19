#pragma once

//Node struct that represents a single element in the Queue
typedef struct{ 
	int size;
	void *data;
	Queue *next;
}QueueNode;

// Get the total size of the queue currently
int getSize();

//Add a node to the start of the queue
void Enqueue(Queue *aQueue, void *data);

//Take a node off the head of the queue since it is a FIFO data struct 
void* Dequeue();

int isEmpty();





