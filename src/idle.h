#include <hardware.h>

void idle_proc_create(unsigned char *track, int track_size, pte_t *user_page_table, UserContext *uctxt);
void DoIdle(void);
