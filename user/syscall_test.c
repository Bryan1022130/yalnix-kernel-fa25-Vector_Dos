#include <sys/types.h> //For u_long
#include <load_info.h> //The struct for load_info
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL
#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC
#include <stdio.h> // for snprintf


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
