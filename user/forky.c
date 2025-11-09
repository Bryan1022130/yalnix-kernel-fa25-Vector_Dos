#include <yalnix.h>
#include <yuser.h>

int main(void) {
	int check = Fork();
	int status = 0;

	if (check == 0) {
		TtyPrintf(0, "Hallo\n");
	} else {
		TtyPrintf(0, "Hello friend\n");
		Wait(&status);
	}
	TtyPrintf(0, "This is the value of the status -> %d and this is my pid ->%d\n", status, GetPid());
	Exit(45);
}
