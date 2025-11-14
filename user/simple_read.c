int main (void) { 
	char buf[10] = {0};
	TtyRead(1, buf, sizeof(buf) - 1);
	TracePrintf(0, "This is the sentence --> %s\n", buf);
}
