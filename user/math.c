#include <yuser.h>

int main(void) {
	int f = Fork();

	if  (f==0) {
		TtyPrintf(2, "I should print first at the child :)\n");
		int x = 10/0;
		TtyPrintf(2, "Did I abort yet?");
	} else {
		Wait(&f);
		TtyPrintf(2,"I should always print second and this is what i got --> %d", f);
		Exit(40);
	}

	//This will never reach
	Exit(30);
}
