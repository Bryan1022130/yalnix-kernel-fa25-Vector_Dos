#include <yalnix.h>
#include <yuser.h>
#include <ylib.h>

int main() {
    TtyPrintf(0, "After Exec: running program B, PID=%d\n", GetPid());

    Delay(2);
    Brk((void *)0x120000);
    TtyPrintf(0, "ProgB finished syscalls succsessfully. \n");

    return 0;
}
