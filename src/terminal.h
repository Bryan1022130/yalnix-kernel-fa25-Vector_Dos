 typdef struct termial{
	 //Terminal Tracking
	 char is_busy;
	 int terminal_num;

	 //Blocked Process Logic
	 PCB *waiting_process;
	 void *waiting_buffer;
	 int message_line_len;

 }Terminal;


void TerminalSetup(void);
void TerminalFree(int tnum);
