#include <hardware.h>
int main (void){
	long int x = 0;
	while(x < 3){
		x++;
		TracePrintf(0, "Hello i am called\n");
	}
}
