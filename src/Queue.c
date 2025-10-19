#include "Queue.h"

//This is probably unnecessary since you can check integers for true or false
//But this is just to make it more clear
#define TRUE 1
#define FALSE 0

//Initialzie the Queue and return it to users
Queue *initializeQueue(){
	Queue *NewQueue = (Queue*) malloc(sizeof(Queue));
	if(NewQueue == NULL){
		TracePrintf(0, "Error with malloc for Queue");
		return NULL;
	}

	NewQueue->size = 0;
	NewQueue->head = NULL;
	NewQueue->tail = NULL;
	return NewQueue;
}

// Create a instance of a node in heap memory
QueueNode *makeNode(void *data){
	QueueNode *aNode = (QueueNode *) malloc(sizeof(QueueNode));

	if(aNode == NULL){
		TracePrintf(0, "Error with malloc in create Node");
		return NULL;
	}

	aNode->data = data;
	aNode->next = NULL;
	return aNode;
}

int getSize(Queue *MQueue){
	return MQueue->size;
}

void Enqueue(Queue *MQueue, void *data){
	// Make a new Node that is added into the Queue
	QueueNode *Node = makeNode(data);
	if(Node == NULL) return;
		
	//If the Queue is empty, make the first node the head node 
	if(isEmpty(MQueue) == TRUE){
		MQueue->head = Node;
		MQueue->tail = Node;
		MQueue->size++;
		return;
	}

	//Otherwise attach the new node to current tail and then update the tail Node
	MQueue->tail->next = Node;
	MQueue->tail = Node;
	MQueue->size++;
	return;

}

void *Dequeue(Queue *MQueue){
	if(isEmpty(MQueue) == TRUE){
		TracePrintf(0, "Error! The Queue is currently empty");
		return NULL;
	}

	//Since a Queue is FIFO, we get rid of the front 
	//We then update to the Node it points to the current heads next
	//Tail stays the same 

	QueueNode *temp = MQueue->head;
	void *tempdata = temp->data;

	MQueue->head = MQueue->head->next;
	MQueue->size--;

	if(MQueue->head == NULL){
		MQueue->tail = NULL;
	}

	free(temp);

	return tempdata;
}

int isEmpty(Queue *MQueue){
	//return 1 for true 
	if(MQueue->size == 0){
		return TRUE;
	}

	return FALSE;
}

QueueNode *peek(Queue *MQueue){
	if(MQueue->head == NULL){
		TracePrintf(0, "The Queue is empty!");
		return NULL;
	}

	return MQueue->head;
}
