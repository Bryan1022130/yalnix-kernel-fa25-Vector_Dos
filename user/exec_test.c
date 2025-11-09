#include <yalnix.h>
#include <yuser.h>
#include <ylib.h>

int main() {
    TtyPrintf(0, "Before Exec: running program A, PID=%d\n", GetPid());
    char *argv[] = {"progB", NULL};
    int rc = Exec("../user/progB", argv);
    TtyPrintf(0, "This should never print! Exec returned %d\n", rc);
    return 0;
}
