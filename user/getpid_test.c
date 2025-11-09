#include <yalnix.h>
#include <yuser.h>

int main(void) {
    int pid = GetPid();
    TtyPrintf(0, "[GetPid Test] My PID is %d\n", pid);
    Exit(0);
}
