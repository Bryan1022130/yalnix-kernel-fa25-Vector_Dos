#include <yalnix.h>
#include <yuser.h>

int main(void)	{
	TtyPrintf(0, "[Tty Test] Hello from PID %d\n", GetPid());
	Exit(0);
}
