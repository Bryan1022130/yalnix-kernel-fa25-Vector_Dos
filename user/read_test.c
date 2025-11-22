#include <yuser.h>

int main (void){
	TtyPrintf(2, "Hello! I will test if i can combine multiple inputs from the same terminal into one buffer\n");
	TtyPrintf(2, "I will read input from terminal 1 and then write the final product to terminal 0\n");
	TtyPrintf(2, "I will now delay for 20 seconds!\n");

	Delay(20);

	char shadow[128];
	TtyRead(1, shadow, sizeof(shadow) - 1);
	TtyPrintf(0, "This is the string i got --> %s\n", shadow);

	Exit(70);
}

