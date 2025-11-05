#include "Queue.h"
#include "process.h"

extern PCB *current_process;

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
	
	PCB *convert = (PCB *)data;
	TracePrintf(0, "You have enqueued and this is state of your PCB PID--> %d\n", convert->pid);
	TracePrintf(0, "You have enqueued and this is state of your PCB PID --> %d\n", convert->pid);
	TracePrintf(0, "You have enqueued and this is state of your PCB PID--> %d\n", convert->pid);


	//If the Queue is empty, make the first node the head node and tail 
	if(isEmpty(MQueue)){
		MQueue->head = Node;
		MQueue->tail = Node;
		MQueue->size++;
		return;
	}

	//Otherwise attach the new node to current tail and then update the tail Node
	MQueue->tail->next = Node;
	MQueue->tail = Node;
	MQueue->size++;
	
	if(convert->pid == 0){
		TracePrintf(0, "This is the IDLE PROCESS\n");
	}else{
		TracePrintf(0, "This is the INIT PROCESS\n");
	}
}

void *Dequeue(Queue *MQueue){
	if(isEmpty(MQueue) == TRUE){
		TracePrintf(0, "Error! The Queue is currently empty");
		return NULL;
	}

	//Since a Queue is FIFO, we get rid of the front 
	//We then update to the Node it points to (current heads next)
	//Tail stays the same 

	QueueNode *temp = MQueue->head;
	void *tempdata = temp->data;

	MQueue->head = MQueue->head->next;
	MQueue->size--;
	
	//Incase we dequeued the last Node
	if(MQueue->head == NULL){
		MQueue->tail = NULL;
	}

	//free since we called malloc, also to avoid memory leak
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
		if(current_process->pid == 0){
			TracePrintf(0, "IDLE CALLED\n");
		}else{
			TracePrintf(0, "INIT CALLED\n");
		}

		TracePrintf(0, "The Queue is empty!\n");
		return NULL;
	}
	QueueNode *temp = MQueue->head;
	PCB *tempdata = temp->data;
	int pid_extract = tempdata->pid; 

	TracePrintf(0, "This is the number of the pid that we extracted  --> %d\n", pid_extract);
	TracePrintf(0, "Okay we are now leaving the peek funciton\n");
	return MQueue->head;
}
