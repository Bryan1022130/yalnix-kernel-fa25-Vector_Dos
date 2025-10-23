#include "memory.h"
#include <stdio.h>

// Global frame tracker (private to memory.c)
static Frame frame_table[MAX_FRAMES];
static int total_frames = 0;

void frames_init(unsigned int pmem_size) {
	/*
	* GOAL: Initialize frame_table so we can track every physical frame.
	*/

	// Step 1: Compute  total_frames = pmem_size /PAGESIZE
	total_frames = pmem_size / PAGESIZE;
	printf("Initializing frame table: %d frames total\n", total_frames);

	// Step 2: Mark kernel-used frames as in_use = 1
	for (int i = 0; i < total_frames; i++) {
        frame_table[i].pfn = i;
        frame_table[i].in_use = 0;
        frame_table[i].owner_pid = -1;  // -1 = kernel/unowned
        frame_table[i].refcount = 0;
    }
	// Step 3: Mark all others as free
	for (int i = 0; i < 256 && i < total_frames; i++) {
        frame_table[i].in_use = 1;
        frame_table[i].owner_pid = -1;
    }

    printf("Frame table ready — %d kernel frames reserved, %d free.\n",
           (total_frames < 256) ? total_frames : 256,
           total_frames - ((total_frames < 256) ? total_frames : 256));
}

int frame_alloc(int owner_pid) {
	/*
	* Find first free frame in frame_table, mark it in use, and return its pfn.
	*/
	for (int i = 0; i < total_frames; i++) {
        if (frame_table[i].in_use == 0) {
            frame_table[i].in_use = 1;
            frame_table[i].owner_pid = owner_pid;
            frame_table[i].refcount = 1;
            return i;
        }
    }

    printf("ERROR: Out of physical frames!\n");
    return -1; // indicates failure
}

void frame_free(int pfn) {
	/*
	* decrement refcnt, if 0 mark free; run helper_force_free(pfn) if needed later.
	*/
	if (pfn < 0 || pfn >= total_frames) {
        printf("ERROR: invalid frame number %d\n", pfn);
        return;
    }

    if (frame_table[pfn].refcount > 0)
        frame_table[pfn].refcount--;

    if (frame_table[pfn].refcount == 0) {
        frame_table[pfn].in_use = 0;
        frame_table[pfn].owner_pid = -1;
    }
}

int SetKernelBrk(void *addr) {
	/*
	* Use frame_alloc() to grow kernel heap or frame_free() to shrink it.
	*/
	 printf("[memory.c] Kernel break moved to %p\n", addr);
    return 0;
}
