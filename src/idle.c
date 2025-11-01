#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL
#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/types.h> //For u_long

/* ===================================================================================================================
 * Idle Function that runs in Kernel Space
 * Continuously calls Pause() to yield CPU.
 * ===================================================================================================================
 */

void DoIdle(void) {
        int count = 0;
        while(1) {
                TracePrintf(1,"Idle loop running (%d)\n", count++);
                Pause();
        }
}




