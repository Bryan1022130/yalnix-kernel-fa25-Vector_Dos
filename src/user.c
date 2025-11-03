#include <hardware.h>
int main (void){
	int x = 0;
	while(x < 5){
		x++;
		TracePrintf(0, "Hello i am called\n");
	}
}
