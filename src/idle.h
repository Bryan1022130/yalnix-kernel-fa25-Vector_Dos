#include <hardware.h>

int idle_proc_create(unsigned char *track, pte_t *user_page_table, UserContext *uctxt);
void DoIdle(void);
