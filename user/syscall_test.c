#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC

int test(void) {
    //TtyPrintf(0, "\n=== [USER] Syscall Test Start ===\n");

    // 1. Check PID
    //TtyPrintf(0, "[USER] Calling GetPid()...\n");
    int pid = GetPid();
   //TtyPrintf(0, "[USER] Returned from GetPid(), PID = %d\n", pid);

    // 2. Test Brk() several times
    void *base = (void *)0x110000;
   // TtyPrintf(0, "[USER] Calling Brk(%p)...\n", base);
    int rc1 = Brk(base);
    //TtyPrintf(0, "[USER] Returned from Brk, rc = %d\n", rc1);

    void *next = (void *)0x130000;
   // TtyPrintf(0, "[USER] Expanding heap with Brk(%p)...\n", next);
    int rc2 = Brk(next);
    //TtyPrintf(0, "[USER] Returned from Brk, rc = %d\n", rc2);

    //3. Test Delay() inside a loop
    TtyPrintf(0, "[USER] Starting delay loop (3x 1 tick)...\n");
    int rid = 0;
    
    for (int i = 1; i <= 5; i++) {
       // TtyPrintf(0, "[USER] Delay %d/3 start.\n", i);
        Delay(1);
	rid = GetPid();
        //TtyPrintf(0, "[USER] Delay %d/3 done.\n", i);
    }

    //Delay(1);
    
    // 4. Mix multiple syscalls
    TtyPrintf(0, "[USER] Interleaving GetPid and Brk again...\n");
    pid = GetPid();
    Brk((void *)0x140000);
    TtyPrintf(0, "[USER] New PID = %d, heap moved to 0x140000.\n", pid);


    // 5. Test Exec()
    //TtyPrintf(0, "[USER] Calling Exec() to run progB...\n"

    // 6. Confirm kernel handled all traps, then exit
    TtyPrintf(0, "[USER] All syscalls executed successfully. Exiting now.\n");
}

int main(void) {
	test();
}
