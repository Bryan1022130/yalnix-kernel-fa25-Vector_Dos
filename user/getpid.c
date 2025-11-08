#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()

int main(void){
	int pid = GetPid();

	//Test Exit
	Exit(100);
}
