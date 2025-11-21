#include <yuser.h>

int main (void) { 
	char *shadow = malloc(1);

	if (shadow == NULL) { 
		TtyPrintf(2, "MALLOC FAILED");
	}else{
		TtyPrintf(3, "This is where malloc is -> %p\n", shadow);
	}

	*shadow = 'a';

	for (int shadoww = 0; shadoww < 120; shadoww++) { 

		char *whisker =  malloc(2048);

		if (whisker == NULL) { 
			TtyPrintf(2, "MALLOC FAILED");
		}else{
			TtyPrintf(3, "This is where malloc is -> %p\n", whisker);
			whisker = 'a';
		}
	}
	for (int shadoww = 0; shadoww < 120; shadoww++) { 

		char hallo[2048] = {0};
		TtyPrintf(2, "I am allocating 2048 bytes and this is the start of my buffer ->%p\n", hallo);
		
		for (int i = 0; i < 1000; i++ ) {
			hallo[i] = 'a';
		}
	}
			

	Exit(50);
}
