//Header files from yalnix_framework && libc library
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL
#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC

//Our Header Files
#include "Queue.h"
#include "trap.h"
#include "memory.h"
#include "process.h"
#include "idle.h"
#include "init.h"
#include "terminal.h"

extern Terminal t_array[NUM_TERMINALS];

void TerminalSetup(void) {
	TracePrintf(0, "We are setting up the Terminal array!\n");
	
	//Clear out and populate the struct
	memset(t_array, 0, NUM_TERMINALS * sizeof(Terminal));

	for (int x = 0; x < NUM_TERMINALS; x++) {
		t_array[x].terminal_num = x;
		t_array[x].transmit_waiting_process = NULL;
		t_array[x].read_waiting_process = NULL;
		t_array[x].one_message_buffer = NULL;
		t_array[x].one_message_length = 0;
		t_array[x].transmit_message_head = NULL;
		t_array[x].input_read_head = NULL;

	}
	TracePrintf(0, "I am done setting up the terminals!\n");
}

void TerminalFree(unsigned int tnum) {
	TracePrintf(0, "We are freeing Terminal %d\n", tnum);

	if (tnum > NUM_TERMINALS) {
		TracePrintf(0, "This is not a valid terminal!\n");
		return;
	}

	//Free all messages nodes from transmit list
	int transmit = 1;
	if (message_free(tnum, transmit) == ERROR) { 
		TracePrintf(0, "There was an error with free messages!\n");
		return;
	}

	//Free all messages from the read list
	int read = 2;
	if (message_free(tnum, read) == ERROR) { 
		TracePrintf(0, "There was an error with free messages!\n");
		return;
	}
	
	//Null out these fields 
	t_array[tnum].read_waiting_process = NULL;
	t_array[tnum].transmit_waiting_process = NULL;

	if (t_array[tnum].one_message_buffer != NULL) { 
		TracePrintf(0, "Freeing the one message buffer!\n");
		free(t_array[tnum].one_message_buffer);
		t_array[tnum].one_message_buffer = NULL; 
	}

	//Finally memset everything [We dont want to delete just clean it]
	memset(&t_array[tnum], 0, sizeof(Terminal));
	t_array[tnum].terminal_num = tnum; //Its number should always stay the same

	TracePrintf(0, "Done! Leaving! We haev freed terminal %d!\n", tnum);
}

int add_message(int terminal, MessageNode *message) {
	if (message == NULL) {
		TracePrintf(0, "The message node that you passed in was NULL\n");
		return ERROR;
	}

	//If there is no other nodes than make it the head
	if (t_array[terminal].transmit_message_head == NULL) {
		TracePrintf(0, "I will now make your message the head node!\n");
		t_array[terminal].transmit_message_head = message;
		message->next = NULL;
		return SUCCESS;
	}

	TracePrintf(0, "I will add this node to the end of the linked list\n");
	MessageNode *current = t_array[terminal].transmit_message_head;
	while (current->next != NULL) {
		current = current->next;
	}

	current->next = message;
	message->next = NULL;
	return SUCCESS;
}

//Remove the head message
MessageNode *remove_message(int terminal) {
	if (t_array[terminal].transmit_message_head == NULL) {
		TracePrintf(0, "RemoveMessage:There is no MessageNode to return :(. Returning NULL!\n");
		return NULL;
	}
	
	//Incase this is the only node in the list 
	if (t_array[terminal].transmit_message_head->next == NULL) {
		TracePrintf(0, "RemoveMessage: We found a message but the message list is now going to be empty!\n");
		MessageNode *node = t_array[terminal].transmit_message_head;
		node->next = NULL;
		t_array[terminal].transmit_message_head = NULL;
		return node;
	}

	TracePrintf(0, "RemoveMessage: Found a message! Updating the head of the list!\n");
	MessageNode *node = t_array[terminal].transmit_message_head;
	node->next = NULL;

	//Update the head of the message linked list
	t_array[terminal].transmit_message_head = t_array[terminal].transmit_message_head->next; 
	return node;
}

void message_check(int terminal) {
	if (t_array[terminal].transmit_message_head == NULL) {
		TracePrintf(0, "Sorry but this terminal has no messages I can read!\n");
		return;
	}

	MessageNode *current = t_array[terminal].transmit_message_head;
	while (current != NULL) {
		TracePrintf(0, "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		TracePrintf(0, "This is the length of the message -> %d\n", current->length);
		TracePrintf(0, "This the the message found --> %s", current->message);
		TracePrintf(0, "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		current = current->next;
	}
}

int message_free(int terminal, int which_list) {
	MessageNode *list_free;
	if(which_list == 1){
		TracePrintf(0, "We are going to be freeing all the transmit messages\n");
		list_free = t_array[terminal].transmit_message_head;
	} else {
		TracePrintf(0, "We are goign to be freeing all the read messages\n");
		list_free = t_array[terminal].input_read_head;
	}

	if(list_free == NULL){
		TracePrintf(0, "Error! There is no messages in the to free!\n");
		return ERROR;
	}

	//Get a pointer to the linked list 
	MessageNode *next; 

	while (list_free != NULL) {
		TracePrintf(0, "We are now going to free all messages\n");
		
		//Get a pointer to the next node now, since we are about to free
		next = list_free->next;

		//Since the message field in a MessageNode calls malloc
		//We should check if its valid and set it free

		if (list_free != NULL) { 
			TracePrintf(0, "We are going to free a message from the struct MessageNode!\n");
			free(list_free->message);
		}

		//Now we can clear everything else since its not dynamically allocated
		memset(list_free, 0, sizeof(MessageNode));

		//Free the current node and advance 
		free(list_free);
		list_free = next;
	}

	if (which_list == 1) {
		TracePrintf(0, "Great! The transmit message head is now NULL!\n");
		t_array[terminal].transmit_message_head = NULL;
	} else {
		TracePrintf(0, "Great! The read message head is now NULL!\n");
		t_array[terminal].input_read_head = NULL;
	}

	TracePrintf(0, "We have now cleared all the messages nodes and free all dynamic memory usuage!\n");
	TracePrintf(0, "The message head is now NULL! Okay. Leaving! Bye!\n");
	return SUCCESS;
}

// ------------------------------------------- Helper functions for the TTyread 

int read_add_message(int terminal, MessageNode *message) {
	if (message == NULL) {
		TracePrintf(0, "The message node that you passed in was NULL\n");
		return ERROR;
	}

	//If there is no other nodes than make it the head
	if (t_array[terminal].input_read_head == NULL) {
		TracePrintf(0, "I will now make your message the head node!\n");
		t_array[terminal].input_read_head = message;
		message->next = NULL;
		return SUCCESS;
	}

	TracePrintf(0, "I will add this node to the end of the linked list\n");
	MessageNode *current = t_array[terminal].input_read_head;
	while (current->next != NULL) {
		current = current->next;
	}

	current->next = message;
	message->next = NULL;
	return SUCCESS;
}

//Remove the head message
MessageNode *read_remove_message(int terminal) {
	if (t_array[terminal].input_read_head == NULL) {
		TracePrintf(0, "There is no MessageNode to return :(. Returning NULL!\n");
		return NULL;
	}
	
	//Incase this is the only node in the list 
	if (t_array[terminal].input_read_head->next == NULL) {
		TracePrintf(0, "We found a message but the message list is now going to be empty!\n");
		MessageNode *node = t_array[terminal].input_read_head;
		t_array[terminal].input_read_head = NULL;
		return node;
	}

	TracePrintf(0, "Found a message! Updating the head of the list!\n");
	MessageNode *node = t_array[terminal].input_read_head;

	//Update the head of the message linked list
	t_array[terminal].input_read_head = t_array[terminal].input_read_head->next;

	node->next = NULL;

	return node;
}

int read_message_free(int terminal) {
	TracePrintf(0, "We are going to free all the messages in the list!\n");

	if(t_array[terminal].input_read_head  == NULL) {
		TracePrintf(0, "Error! There is no messages in my list to free!\n");
		return ERROR;
	}

	//Get a pointer to the linked list 
	MessageNode *current = t_array[terminal].input_read_head;
	MessageNode *next; 

	while (current != NULL) {
		TracePrintf(0, "HELLO USER : We are now going to free a message!\n");
		TracePrintf(0, "We are first going to clear out the MessageNode\n");

		next = current->next;
		memset(current, 0, sizeof(MessageNode));
		free(current);
		current = next;
	}

	t_array[terminal].input_read_head = NULL;
	TracePrintf(0, "We have now cleared all the messages nodes and free them!\n");
	TracePrintf(0, "The message head is now NULL! Okay. Leaving! Bye!\n");

	return SUCCESS;
}

