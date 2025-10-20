#include "memory.h"

// Global frame tracker (private to memory.c)
static Frame frame_table[MAX_FRAMES];
static int total_frames = 0;

void frames_init(unsigned int pmem_size) {
    /*
     * GOAL: Initialize frame_table so we can track every physical frame.
     * 1. Compute total_frames = pmem_size / PAGESIZE.
     * 2. Mark kernel-used frames as in_use = 1.
     * 3. Mark all others as free.
     */
}

int frame_alloc(int owner_pid) {
    /*
     * Find first free frame in frame_table, mark it in use, and return its pfn.
     */
}

void frame_free(int pfn) {
    /*
     * decrement refcnt, if 0 mark free; run helper_force_free(pfn) if needed later.
     */
}

int SetKernelBrk(void *addr) {
    /*
     * Use frame_alloc() to grow kernel heap or frame_free() to shrink it.
     */
}
