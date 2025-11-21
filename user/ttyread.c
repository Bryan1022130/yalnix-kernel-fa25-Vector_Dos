#include <yuser.h>

int main() {
    char buf[10];  // Small buffer to test partial reads
    int n;
    
    TracePrintf(1, "=== TtyRead Test Program ===\n");
    
    // Test 1: First read
    TracePrintf(1, "\nTest 1: Reading with 10-byte buffer\n");
    n = TtyRead(0, buf, 10);
    TracePrintf(1, "\n=== Tests Complete ===\n");
    return 0;
}
