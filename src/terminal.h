typedef struct messagenode {
	//User input
	char *message;

	//Length of the input
	unsigned int length;

	//NextNode
	struct messagenode *next;

	//if it has been read
	unsigned char not_read;
}MessageNode;

typedef struct termial {
	 //Terminal Tracking
	 char is_busy;
	 int terminal_num;

	 //Blocked Process Logic {Might have to change later}
	 PCB *waiting_process;
	 void *waiting_buffer;
	 int message_line_len;

	 //Linked-List to store messages in chunks
	 MessageNode *message_head;

}Terminal;

//Message tracking logic
int add_message(int terminal, MessageNode *message);
MessageNode *remove_message(int terminal);
void message_check(int terminal);
int message_free(int terminal);

//Terminal functions
void TerminalSetup(void);
void TerminalFree(int tnum);
