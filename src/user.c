#include <hardware.h>
int main (void){
	long int x = 0;
	while(x < 30000000000000){
		x++;
		TracePrintf(0, "Hello i am called\n");
	}
}
