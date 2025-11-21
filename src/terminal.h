typedef struct messagenode {
	//User input
	char *message;

	//Length of the input
	unsigned int length;

	//NextNode
	struct messagenode *next;

}MessageNode;

typedef struct termial {
	 //Terminal Tracking
	 int terminal_num;

	 //Blocked process for transmitting 
	 PCB *transmit_waiting_process;

	 //Blocked process for reading
	 PCB *read_waiting_process;

	 //For when we want to transmit one message
	 char *one_message_buffer;
	 int one_message_length;

	 //Linked-List to store messages in chunks
	 MessageNode *transmit_message_head;

	 //Linked-List to keep track of user input {For TtyRead}
	 MessageNode *input_read_head;

}Terminal;

//Message tracking logic
int add_message(int terminal, MessageNode *message);
MessageNode *remove_message(int terminal);
void message_check(int terminal);
int message_free(int terminal, int which_list);

//Message Tracking user input
int read_add_message(int terminal, MessageNode *message);
MessageNode *read_remove_message(int terminal);
int read_message_free(int terminal);

//Terminal functions
void TerminalSetup(void);
void TerminalFree(unsigned int tnum);
