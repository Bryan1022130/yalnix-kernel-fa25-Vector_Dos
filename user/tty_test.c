#include <yalnix.h>
#include <yuser.h>

int main(void)	{
	int reap = 2;
	int rc = Fork();

	if (rc==0) { 
		TtyPrintf(2, "[Tty Test] Hello from PID %d\n", GetPid());
	} else {
		TtyPrintf(2, "[Tty Test] I am the parent and I will wait for my child :)\n");
		Wait(&reap);
		TtyPrintf(2, "[Tty Test] I am the parent and I waited for my child :)\n");
		TtyPrintf(2, "[Tty Test] This is the value i got from wait -> %d\n", reap);

		if (reap == 2) {
			while (1) { 
				TtyPrintf(0, "[Tty Test] If this is displayed than wait is incorrect and I didn't reap the child status correctly!\n");
				TtyPrintf(0, "[Tty Test] This should never print!\n");
			}
		}

	}

	TtyPrintf(3, "[Tty Test] Hello this should print twice! This is the process calling -> %d\n", GetPid());
	char buf[8] = {0};
	TtyRead(0, buf, sizeof(buf) - 1);
	TtyPrintf(1, "[Tty Test] This is the string %s and this is the pid of this calling process %d\n", buf, GetPid());
	Exit(43);
}
