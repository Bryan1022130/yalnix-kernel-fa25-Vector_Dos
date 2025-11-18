#include <yuser.h>

int main(void) {
    int status;
    int pid1, pid2;
    
    pid1 = Fork();
    if (pid1 == 0) {
        TtyPrintf(0, "Child 1 with PID %d\n", GetPid());
        Exit(10);
    }

    Delay(5);
    
    pid2 = Fork();
    if (pid2 == 0) {
        TtyPrintf(0, "Child 2 with PID %d\n", GetPid());
        Exit(20);
    }
    
    Wait(&status);
    TtyPrintf(0, "First wait returned status: %d\n", status);
    
    Wait(&status);
    TtyPrintf(0, "Second wait returned status: %d\n", status);
    
    Exit(0);
}
