#include <yalnix.h>
#include <yuser.h>

int main(void)	{
	int reap;
	int rc = Fork();

	if (rc==0) { 
		TtyPrintf(2, "[Tty Test] Hello from PID %d\n", GetPid());
	} else {
		TtyPrintf(2, "[Tty Test] I am the parent and I will wait for my child :)\n");
		Wait(&reap);
		TtyPrintf(2, "[Tty Test] I am the parent and I waited for my child :)\n");

	}

	TtyPrintf(3, "[Tty Test] Hello this should print twice %d\n", GetPid());

	char buf[8] = {0};
	TtyPrintf(1,"[Tty Test] Process %d is calling read and should be first to get message\n", GetPid());
	TtyRead(0, buf, sizeof(buf) - 1);
	TtyPrintf(1, "[Tty Test] This is the string %s and this is the pid of this calling process %d\n", buf, GetPid());
	Exit(43);
}
