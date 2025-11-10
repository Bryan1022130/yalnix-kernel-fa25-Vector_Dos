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

void TerminalSetup(void){
	TracePrintf(0, "We are setting up the Terminal array!\n");
	
	//Clear out and populate the struct
	memset(t_array, 0, NUM_TERMINALS * sizeof(Terminal));
	for (int x = 0; x < NUM_TERMINALS; x++) {
		t_array[x].is_busy = 0;
		t_array[x].terminal_num = x;
		t_array[x].waiting_process = NULL;
		t_array[x].waiting_buffer = 0;
		t_array[x].message_line_len = 0;
		t_array[x].message_head = NULL;
	}
	TracePrintf(0, "I am done setting up the terminals!\n");
}

void TerminalFree(int tnum) {
	TracePrintf(0, "We are freeing Terminal %d\n", tnum);

	if(tnum < 0 || tnum > NUM_TERMINALS){
		TracePrintf(0, "This is not a valid terminal!\n");
		Halt();//Testing right now
		return;
	}

	//Free all messages nodes
	if (message_free(tnum) == ERROR) { 
		TracePrintf(0, "There was an error with free messages!\n");
		return;
	}

	memset(&t_array[tnum], 0, sizeof(Terminal));
	TracePrintf(0, "Done! Leaving now!\n");
}

int add_message(int terminal, MessageNode *message) {
	if (message == NULL) {
		TracePrintf(0, "The message node that you passed in was NULL\n");
		return ERROR;
	}

	//If there is no other nodes than make it the head
	if (t_array[terminal].message_head == NULL) {
		TracePrintf(0, "I will now make your message the head node!\n");
		t_array[terminal].message_head = message;
		message->next = NULL;
		return SUCCESS;
	}

	TracePrintf(0, "I will add this node to the end of the linked list\n");
	MessageNode *current = t_array[terminal].message_head;
	while (current->next != NULL) {
		current = current->next;
	}

	current->next = message;
	message->next = NULL;
	return SUCCESS;
}

//Remove the head message
MessageNode *remove_message(int terminal){
	if (t_array[terminal].message_head == NULL) {
		TracePrintf(0, "There is no MessageNode to return :(. Returning NULL!\n");
		return NULL;
	}
	
	//Incase this is the only node in the list 
	if (t_array[terminal].message_head->next == NULL) {
		TracePrintf(0, "We found a message but the message list is now going to be empty!\n");
		MessageNode *node = t_array[terminal].message_head;
		node->next = NULL;
		t_array[terminal].message_head = NULL;
		return node;
	}

	TracePrintf(0, "Found a message! Updating the head of the list!\n");
	MessageNode *node = t_array[terminal].message_head;
	node->next = NULL;

	//Update the head of the message linked list
	t_array[terminal].message_head = t_array[terminal].message_head->next; 
	return node;
}

void message_check(int terminal) {
	if (t_array[terminal].message_head == NULL) {
		TracePrintf(0, "Sorry but this terminal has no messages I can read!\n");
		return;
	}

	MessageNode *current = t_array[terminal].message_head;
	while (current != NULL) {
		TracePrintf(0, "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		TracePrintf(0, "This is the length of the message -> %d\n", current->length);
		TracePrintf(0, "This the the message found --> %s", current->message);
		TracePrintf(0, "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		current = current->next;
	}
}

int message_free(int terminal) {
	TracePrintf(0, "We are going to free all the messages in the list!\n");
	if(t_array[terminal].message_head == NULL){
		TracePrintf(0, "Error! There is no messages in my list to free!\n");
		return ERROR;
	}

	//Get a pointer to the linked list 
	MessageNode *current = t_array[terminal].message_head;
	MessageNode *next; 

	while (current != NULL) {
		TracePrintf(0, "HELLO USER : We are now going to free a message!\n");
		TracePrintf(0, "We are first going to clear out the MessageNode\n");

		next = current->next;
		memset(current, 0, sizeof(MessageNode));
		free(current);
		current = next;
	}

	t_array[terminal].message_head = NULL;
	TracePrintf(0, "We have now cleared all the messages nodes and free them!\n");
	TracePrintf(0, "The message head is now NULL! Okay. Leaving! Bye!\n");

	return SUCCESS;
}

