/* Test Terminal I/O */
#include <yuser.h>
#include <string.h>

void test_terminal_io(void) {
  TtyPrintf(0, "=== Testing Terminal I/O ===\n");
  
  char *message = "Hello from Yalnix! Type something and press Enter:\n";
  int tty_id = 3;  
  
  TtyPrintf(0, "Writing to terminal %d\n", tty_id);
  int bytes_written = TtyWrite(tty_id, message, strlen(message));
  
  if (bytes_written == ERROR) {
      TracePrintf(0, "TtyWrite failed\n");
      return;
  }
  
  TtyPrintf(0, "Wrote %d bytes to terminal %d\n", bytes_written, tty_id);
  
  // Test TtyRead
  char buffer[100] ;
  TtyPrintf(0, "Reading from terminal %d\n", tty_id);
  int bytes_read = TtyRead(tty_id, buffer, sizeof(buffer) - 1);
  
  if (bytes_read == ERROR) {
      TracePrintf(0, "TtyRead failed\n");
      return;
  }
  
  // Null-terminate the buffer
  buffer[bytes_read] = '\0';
  
  TtyPrintf(0, "Read %d bytes from terminal %d: '%s'\n", bytes_read, tty_id, buffer);
  
  // Echo the input back to the terminal
  char message2[] = "You typed: ";
  TtyWrite(tty_id, message2, strlen(message2));
  TtyWrite(tty_id, buffer, bytes_read);

  Exit(67);
}


int main(void) { 
	test_terminal_io();
}

