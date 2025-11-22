#include <yalnix.h>
#include <yuser.h>

int lock;

void child(int id) {
    TtyPrintf(0, "[child %d] started, trying to acquire lock...\n", id);
    Acquire(lock);
    TtyPrintf(0, "[child %d] acquired lock!\n", id);

    Delay(5);  // hold lock briefly
    TtyPrintf(0, "[child %d] releasing lock...\n", id);
    Release(lock);

    Exit(40);
}

int main() {
    if (LockInit(&lock) < 0) {
        TtyPrintf(0, "LockInit failed\n");
        Exit(1);
    }

    TtyPrintf(0, "[parent] created lock %d\n", lock);

    int c1 = Fork();
    if (c1 == 0) child(1);

    int c2 = Fork();
    if (c2 == 0) child(2);

    int c3 = Fork();
    if (c3 == 0) child(3);

    TtyPrintf(0, "[parent] acquiring lock first...\n");
    Acquire(lock);
    TtyPrintf(0, "[parent] has lock, holding for a moment...\n");

    Delay(10);  // force children to block behind us
    TtyPrintf(0, "[parent] releasing lock now\n");
    Release(lock);

    int status;
    TtyPrintf(0, "[parent] I will wait for my child\n");
    Wait(&status);
    TtyPrintf(0, "This the exit value -> %d\n", status);

    TtyPrintf(0, "[parent] I will wait for my child\n");
    Wait(&status);
    TtyPrintf(0, "This the exit value -> %d\n", status);

    TtyPrintf(0, "[parent] I will wait for my child\n");
    Wait(&status);
    TtyPrintf(0, "This the exit value -> %d\n", status);

    TtyPrintf(0, "[parent] all children exited\n");
    Exit(50);
}
