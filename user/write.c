#include <yalnix.h>
#include <ylib.h>
#include <yuser.h>

int main(void) {
	void *buffer = "HALLOOO FROM THE OTHER SIDE";
	int ret = TtyWrite(3, buffer, 28);

	if(ret < 0) { 
		Exit(-1);
	}

	Exit(56);
}
