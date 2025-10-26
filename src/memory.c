#include "memory.h"
#include <stdio.h>
#include <yalnix.h>
#include <hardware.h> 
#include <ykernel.h> 

static Frame frame_table[MAX_FRAMES];
static int total_frames = 0;

void frames_init(unsigned int pmem_size) {

	// Step 1: Compute  total_frames = pmem_size /PAGESIZE
	total_frames = pmem_size / PAGESIZE;

	// Step 2: Mark kernel-used frames as in_use = 1
	for (int i = 0; i < total_frames; i++) {
        frame_table[i].pfn = i;
        frame_table[i].in_use = 0;
        frame_table[i].owner_pid = -1;  // -1 = kernel/unowned
        frame_table[i].refcount = 0;
    }
	// Step 3: Mark all others as free
	unsigned long first_kernel_page = (unsigned long)_first_kernel_text_page;
	unsigned long first_unused_page = (unsigned long)_orig_kernel_brk_page;

   	for (unsigned long i = first_kernel_page; i < first_unused_page; i++) {
		if (i < total_frames) {
			frame_table[i].in_use = 1;
            		frame_table[i].refcount = 1; // Kernel pages are never "freed"
        	}
    	}

  	unsigned long stack_base_page = KERNEL_STACK_BASE >> PAGESHIFT;
   	unsigned long stack_limit_page = KERNEL_STACK_LIMIT >> PAGESHIFT;

    	for (unsigned long i = stack_base_page; i < stack_limit_page; i++) { 
		if (i < total_frames) {
			frame_table[i].in_use = 1;
			frame_table[i].refcount = 1;
        	}
   	 }

	TracePrintf(1, "Frame table initialization done\n");
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
