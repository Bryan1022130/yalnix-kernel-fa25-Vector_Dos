#include <yalnix.h>
#include <stdio.h>
#include <hardware.h>
#include <ylib.h>
#include <yuser.h>

int main(void) {
	while(1){
		TracePrintf(0, "\n\n");
		TracePrintf(0, "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		TracePrintf(0, "We are tracing printing and waiting for a clock trap to execute (INIT) :)\n");
		TracePrintf(0, "We are tracing printing and waiting for a clock trap to execute (INIT) :)\n");
		TracePrintf(0, "We are tracing printing and waiting for a clock trap to execute (INIT):)\n");
		TracePrintf(0, "We are tracing printing and waiting for a clock trap to execute (INIT) :)\n");
		TracePrintf(0, "We are tracing printing and waiting for a clock trap to execute (INIT):)\n");
		TracePrintf(0, "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		TracePrintf(0, "\n\n");
		Pause();
	}
}

