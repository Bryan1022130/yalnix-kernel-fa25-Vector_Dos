#include "Queue.h"
#include "process.h"

extern PCB *current_process;

#define TRUE 1
#define FALSE 0

//Initialzie the Queue and return it to users
Queue *initializeQueue(void) {
	Queue *NewQueue = (Queue*) malloc(sizeof(Queue));
	if (NewQueue == NULL) {
		TracePrintf(0, "Error with malloc for Queue");
		return NULL;
	}

	NewQueue->size = 0;
	NewQueue->head = NULL;
	NewQueue->tail = NULL;
	return NewQueue;
}

// Create a instance of a node in heap memory
QueueNode *makeNode(void *data) {
	QueueNode *aNode = (QueueNode *) malloc(sizeof(QueueNode));

	if (aNode == NULL) {
		TracePrintf(0, "Error with malloc in create Node");
		return NULL;
	}

	aNode->data = data;
	aNode->next = NULL;
	return aNode;
}

int getSize(Queue *MQueue) {
	return MQueue->size;
}

void Enqueue(Queue *MQueue, void *data) {
	// Make a new Node that is added into the Queue
	QueueNode *Node = makeNode(data);
	if(Node == NULL) return;
	
	PCB *convert = (PCB *)data;
	TracePrintf(0, "You have enqueued and this the pid of the process going into the queue--> %d\n", convert->pid);


	//If the Queue is empty, make the first node the head node and tail 
	if(isEmpty(MQueue)) {
		MQueue->head = Node;
		MQueue->tail = Node;
		MQueue->size++;
		return;
	}

	//Otherwise attach the new node to current tail and then update the tail Node
	MQueue->tail->next = Node;
	MQueue->tail = Node;
	MQueue->size++;	
}

void *Dequeue(Queue *MQueue) {
	if(isEmpty(MQueue) == TRUE){
		TracePrintf(0, "Error! The Queue is currently empty");
		return NULL;
	}

	//Since a Queue is FIFO, we get rid of the front 
	QueueNode *temp = MQueue->head;
	void *tempdata = temp->data;

	MQueue->head = MQueue->head->next;
	MQueue->size--;
	
	//Incase we dequeued the last Node
	if(MQueue->head == NULL) {
		MQueue->tail = NULL;
	}

	//free since we called malloc, also to avoid memory leak
	free(temp);

	return tempdata;
}

int isEmpty(Queue *MQueue) {
	if(MQueue->size == 0) {
		return TRUE;
	}

	return FALSE;
}

QueueNode *peek(Queue *MQueue) {
	if (MQueue->head == NULL) {
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

void remove_data(Queue *MQueue, void *find_data) {
	TracePrintf(0, "Hello! I will look for node to remove!\n");

	if (isEmpty(MQueue)) {
		TracePrintf(0, "ERROR! The queue that you passed in was empty!\n");
		return;
	}

	if (MQueue->head == NULL) {
		TracePrintf(0, "Error the head is NULL!\n");
		return;
	}
	
	if ((PCB *)MQueue->head->data == (PCB *)find_data) {
		TracePrintf(0, "I found the PCB at the head! I am going to Dequeue.\n");
		Dequeue(MQueue);
		return;
	}

	//Loop through all nodes
	QueueNode *prev = MQueue->head;
	QueueNode *current = MQueue->head->next;

	while(current) {
		if ((PCB *)current->data == (PCB *)find_data) {
			TracePrintf(0, "Great I found your process! I will now remove it ;)\n");
			prev->next = current->next;
			if(current == MQueue->tail){
				MQueue->tail = prev;
			}
			free(current);
			MQueue->size--;
			return;
		}
		//Advance to next node
		prev = current;
		current = current->next;
	}
	TracePrintf(0, "I could not find the process in the Queue! [THIS MAY BE AN ERROR]\n");
}

int in_queue(Queue *MQueue, void *find_data) {
	TracePrintf(0, "Hello! I will check if I have the node!\n");

	if (isEmpty(MQueue)) {
		TracePrintf(0, "ERROR! The queue that you passed in was empty!\n");
		return 0;
	}

	if (MQueue->head == NULL) {
		TracePrintf(0, "Error the head is NULL!\n");
		return 0;
	}
	
	if ((PCB *)MQueue->head->data == (PCB *)find_data) {
		TracePrintf(0, "I found the PCB at the head!\n");
		return 1;
	}

	//Loop through all nodes
	QueueNode *current = MQueue->head;

	while(current) {
		if ((PCB *)current->data == (PCB *)find_data) {
			TracePrintf(0, "Great I found your process!\n");
			return 1;
		}
		//Advance to next node
		current = current->next;
	}

	TracePrintf(0, "I could not find the process in the Queue! [THIS MAY BE AN ERROR]\n");
	return 0;
}
