#include <yalnix.h>
#include <yuser.h>

int main(void){
	TtyPrintf(0, "[Delay Test] Starting delay of 3 ticks...\n");
	Delay(3);
	TtyPrintf(0, "[Delay Test] Finished delay.\n");
	Exit(0);
}
