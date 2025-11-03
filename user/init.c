#include <yalnix.h>
#include <stdio.h>

int main(void) {
    TtyPrintf(0, "Hello from userland! The kernel made it here :)\n");
    Pause();
    return 0;
}

