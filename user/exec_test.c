#include <yalnix.h>
#include <yuser.h>
#include <ylib.h>

int main() {
    TtyPrintf(0, "Before Exec: running program A, PID=%d\n", GetPid());
    char *argv[] = {"../user/progB", NULL};
    int rc = Exec(argv[0], argv);
    TtyPrintf(0, "This should never print! Exec returned %d\n", rc);
    Exit(99);
}
