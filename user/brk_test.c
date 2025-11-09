#include <yalnix.h>
#include <yuser.h>

int main(void){
	void *addr1 = (void *)0x120000;
	void *addr2 = (void *)0x140000;
	int rc1 = Brk(addr1);
	int rc2 = Brk(addr2);
	TtyPrintf(0, "[Brk Test] Brk()0x120000)=%d, Brk()0x140000)=d%\n", rc1, rc2);
	Exit(0);
}
