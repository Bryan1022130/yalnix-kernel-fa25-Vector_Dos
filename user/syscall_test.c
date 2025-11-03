#include <yalnix.h>
#include <stdio.h>

int main(void) {
    TtyPrintf(0, "=== Syscall Test Start ===\n");

    int pid = GetPid();
    TtyPrintf(0, "My PID is %d\n", pid);

    int rc = Brk((void *)0x120000);
    TtyPrintf(0, "Brk returned %d\n", rc);

    TtyPrintf(0, "Testing Delay(2)...\n");
    Delay(2);
    TtyPrintf(0, "Delay finished.\n");

    void *newbrk = (void *)0x120000;
    Brk(newbrk);
    TtyPrintf(0, "Brk moved heap to %p\n", newbrk);

    TtyPrintf(0, "Exiting now.\n");
    Exit(0);
    return 0;   // won’t reach here
}
