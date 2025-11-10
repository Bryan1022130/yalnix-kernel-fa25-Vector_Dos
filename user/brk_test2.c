#include <yalnix.h>
#include <yuser.h>

int main(void){
        void *normal1 = (void *)0x120000;
        void *normal2 = (void *)0x140000;
        void *too_low = (void *)0x1000;
        void *too_high = (void *)0x500000;

        int rc1 = Brk(normal1);
        int rc2 = Brk(normal2);
        int rc3 = Brk(too_low);
        int rc4 = Brk(too_high);

        TtyPrintf(0, "[Brk Test] normal1=%d, normal2=%d, too_low=%d, too_high=%d \n", rc1, rc2, rc3, rc4);
        Exit(0);
}

