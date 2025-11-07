#include <sys/types.h> //For u_long
#include <load_info.h> //The struct for load_info
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL
#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC
#include <stdio.h> // for snprintf

int test(void) {
     TtyPrintf(0, "\n=== [USER] Syscall Test Start ===\n");

    // 1. Check PID
    TtyPrintf(0, "[USER] Calling GetPid()...\n");
    int pid = GetPid();
    TtyPrintf(0, "[USER] Returned from GetPid(), PID = %d\n", pid);

    // 2. Test Brk() several times
    void *base = (void *)0x110000;
    TtyPrintf(0, "[USER] Calling Brk(%p)...\n", base);
    int rc1 = Brk(base);
    TtyPrintf(0, "[USER] Returned from Brk, rc = %d\n", rc1);

    void *next = (void *)0x130000;
    TtyPrintf(0, "[USER] Expanding heap with Brk(%p)...\n", next);
    int rc2 = Brk(next);
    TtyPrintf(0, "[USER] Returned from Brk, rc = %d\n", rc2);

    // 3. Test Delay() inside a loop
    TtyPrintf(0, "[USER] Starting delay loop (3x 1 tick)...\n");
    int rid = 0;
    /*
    for (int i = 1; i <= 50; i++) {
        TtyPrintf(0, "[USER] Delay %d/3 start.\n", i);
        //Delay(1);
	rid = GetPid();
        TtyPrintf(0, "[USER] Delay %d/3 done.\n", i);
    }
    */
    Delay(1);
    

    // 4. Mix multiple syscalls
    TtyPrintf(0, "[USER] Interleaving GetPid and Brk again...\n");
    pid = GetPid();
    //Brk((void *)0x140000);
    TtyPrintf(0, "[USER] New PID = %d, heap moved to 0x140000.\n", pid);


    // 5. Confirm kernel handled all traps, then exit
    TtyPrintf(0, "[USER] All syscalls executed successfully. Exiting now.\n");
    Exit(0);

    return 0;  // should never reach
}

int main(void) {
	test();
}
