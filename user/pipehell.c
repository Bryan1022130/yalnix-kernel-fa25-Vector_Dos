#include <yuser.h>

char *name = "pipehell";

int pipe_id;
int pipe_id2;

#define PIPE_BUFFER 256

void
writer(int num, char *buf, int len) {
  int rc = 42;
  TracePrintf(0,"writer %d writing\n",num);

  rc = PipeWrite(pipe_id, buf, len);
  TracePrintf(0,"writer %d rc = %d (%d)\n",num, rc,len);

  TracePrintf(0,"writer %d sleeping\n",num);

  Delay(4);
  Exit(6);
}

void
reader(int num, int len) {
  int rc = 42;
  char buf[PIPE_BUFFER];
  TracePrintf(0,"reader %d reading\n",num);

  rc = PipeRead(pipe_id, buf, len);
  TracePrintf(0,"reader %d rc = %d (%d)\n",num, rc,len);
  buf[len] = 0x00;
  TracePrintf(0,"reader %d got [%s]\n",num, buf);

  TracePrintf(0,"reader %d sleeping\n",num);

  Delay(4);
  Exit(7);
}


void
writer2(int num, char *buf, int len) {
  int rc = 42;


  TracePrintf(0,"writer %d writing for non-zero pipe\n",num);

  rc = PipeWrite(pipe_id2, buf, len);
  TracePrintf(0,"writer %d rc = %d (%d)\n",num, rc,len);

  TracePrintf(0,"writer %d sleeping\n",num);

  Delay(4);
  Exit(8);
}

void
reader2(int num, int len) {
  int rc = 42;
  char buf[PIPE_BUFFER];


  TracePrintf(0,"reader %d reading for non-zero pipe\n",num);

  rc = PipeRead(pipe_id2, buf, len);
  TracePrintf(0,"reader %d rc = %d (%d)\n",num, rc,len);
  buf[len] = 0x00;
  TracePrintf(0,"reader %d got [%s]\n",num, buf);

  TracePrintf(0,"reader %d sleeping\n",num);

  Delay(4);
  Exit(9);
}

int main(void) {
	int rc;
	int rc2;

	TracePrintf(0, "-------------------------------------------------\n");
	TracePrintf(0, "%s: Hello, world!  init is pid %d\n",name); 
	
  	rc = PipeInit(&pipe_id);
	rc2 = PipeInit(&pipe_id2);

	TracePrintf(0, "PipeInit (1) is expected to be 0 this is its value --> %d || PipeInit (2) is expected to be 1 and this is its value -> %d\n", pipe_id, pipe_id2);
	
	//First case: Simlpe read and write {Both should exit}
	rc = Fork();
	if (rc==0) {
		TracePrintf(0, "I am going to write a big buffer to block other write calls!\n");
		char *message = "Heavens! what a virulent attack! replied the prince, not in the least disconcerted by this reception.He had just entered, wearing an embroidered court uniform, knee breeches, and shoes, and had stars on his breast and a angel";
		writer(1, message, 227);
	}

	rc = Fork();
	if (rc == 0){
		TracePrintf(0, "I am going to read up the end of the second sentence!\n");
		reader(1, 32); 
	}
	
	TracePrintf(0, "I will now delay the parent for 12 ticks\n");
	Delay(12);
	TracePrintf(0, "The parent has woken up for case 1!\n");

	//Second case: Write a buffer that is 122 bytes and should block since it would be over PIPE_BUFFER_LEN; 
	//read a large amount {200};
	//blocked write buffer should unblock
	//read for a large amount to clear the buffer
	
	rc = Fork();
	if (rc==0) {
		TracePrintf(0, "We should expect process %d to be blocked until the next proc reads!\n", rc);
		char *overflow = "corresponding type value used by the hardware to index into the interrupt vector table in order to find the address of the";
		writer(2, overflow, 122);
	}

	rc = Fork();
	if (rc==0) { 
		TracePrintf(0, "Okay process %d will now read a large amount (200 bytes)!\n", rc);
		reader(2, 200);
	}
	
	rc = Fork();
	if (rc == 0) { 
		TracePrintf(0, "Great! Process %d will not read the rest of the input!\n", rc);
		reader(3, PIPE_BUFFER);
	}

	TracePrintf(0, "I will now delay the parent for 15 ticks\n");
	Delay(15);
	TracePrintf(0, "The parent has woken up for case 2!\n");

	//Third Case: test logic across 2 pipes reading and writing
	//Both fill up the pipe buffer
	//Both do another write which blocks
	//Both read 100 bytes
	//Both read thr rest of the buffer
	
	rc = Fork();
	if (rc == 0) {
	    char *fill_buf = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQR";
	    writer(3, fill_buf, 256);
	}

	rc2 = Fork();
	if (rc2 == 0) {
	    char *fill_buf = "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345";
	    writer2(1, fill_buf, 256);
	}

	TracePrintf(0, "I will now delay the parent for 3 ticks\n");
	Delay(3);
	TracePrintf(0, "The parent has woken up for case 3!\n");

	rc = Fork();
	if (rc==0) {
	    char *block_buf = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
	    writer(4, block_buf, 50);
	}

	rc2 = Fork();
	if (rc2==0) {
	    char *block_buf = "YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY";
	    writer2(2, block_buf, 50);
	}

	TracePrintf(0, "I will now delay the parent for 3 ticks\n");
	Delay(3);
	TracePrintf(0, "The parent has woken up for case 3!\n");

	rc = Fork();
	if (rc==0) {
	    reader(4, 100);
	}

	rc2 = Fork();
	if (rc2==0) {
	    reader2(1, 100);
	}

	TracePrintf(0, "I will now delay the parent for 5 ticks\n");
	Delay(5);
	TracePrintf(0, "The parent has woken up for case 3!\n");

	rc = Fork();
	if (rc==0) {
	    reader(5, 256);
	}

	rc2 = Fork();
	if (rc2==0) {
	    reader2(2, 256);
	}
	
	TracePrintf(0, "This is the final delay for parent!\n");
	Delay(20);

	TracePrintf(0, "All test are done!");
	Exit(40);
}

