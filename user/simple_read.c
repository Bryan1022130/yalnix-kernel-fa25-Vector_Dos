#include <yuser.h>

int main (void) { 
	char buf[10] = {0};
	TtyRead(1, buf, sizeof(buf) - 1);
	TtyPrintf(3, "This is the sentence --> %s\n", buf);
	Exit(98);
}
