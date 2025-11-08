#include <yalnix.h>
#include <yuser.h>

int main (void){
	int check = Fork();

	if(check == 0){
		void *addr = (void *)0x150000;
		Brk(addr);
	}

	void *addr = (void *)0x130000;
	Brk(addr);
}

