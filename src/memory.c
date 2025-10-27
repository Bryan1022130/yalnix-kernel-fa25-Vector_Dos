#include "memory.h"
#include <stdio.h>
#include <yalnix.h>
#include <hardware.h> 
#include <ykernel.h> 

#define FRAME_COUNT 1000


static Frame frame_table[FRAME_COUNT];
static int total_frames = 0;


void frames_init(unsigned int pmem_size) {
    // Step 1: Compute total_frames = pmem_size / PAGESIZE
    total_frames = pmem_size / PAGESIZE;
    
    // Step 2: Initialize all frames as free first
    for (int i = 0; i < total_frames; i++) {
        frame_table[i].pfn = i;
        frame_table[i].in_use = 0;
        frame_table[i].owner_pid = -1;  // -1 = kernel/unowned
        frame_table[i].refcount = 0;
    }
    
    // Step 3: Mark LOW MEMORY (frames 0 to _first_kernel_text_page-1) as used
    // This is CRITICAL - these frames are used by hardware/bootloader
    unsigned long first_kernel_page = (unsigned long)_first_kernel_text_page;
    
    for (unsigned long i = 0; i < first_kernel_page; i++) {
        if (i < total_frames) {
            frame_table[i].in_use = 1;
            frame_table[i].owner_pid = -1;  // Kernel owns it
            frame_table[i].refcount = 1;
        }
    }
    
    // Step 4: Mark kernel text/data/heap as used
    unsigned long first_unused_page = (unsigned long)_orig_kernel_brk_page;
    
    for (unsigned long i = first_kernel_page; i < first_unused_page; i++) {
        if (i < total_frames) {
            frame_table[i].in_use = 1;
            frame_table[i].owner_pid = -1;
            frame_table[i].refcount = 1;
        }
    }
    
    // Step 5: Mark kernel stack as used
    unsigned long stack_base_page = KERNEL_STACK_BASE >> PAGESHIFT;
    unsigned long stack_limit_page = KERNEL_STACK_LIMIT >> PAGESHIFT;
    
    for (unsigned long i = stack_base_page; i < stack_limit_page; i++) {
        if (i < total_frames) {
            frame_table[i].in_use = 1;
            frame_table[i].owner_pid = -1;
            frame_table[i].refcount = 1;
        }
    }
    
    TracePrintf(1, "Frame table initialization done\n");
    TracePrintf(1, "  Marked frames 0-%lu as low memory (reserved)\n", first_kernel_page - 1);
    TracePrintf(1, "  Marked frames %lu-%lu as kernel text/data\n", first_kernel_page, first_unused_page - 1);
    TracePrintf(1, "  Marked frames %lu-%lu as kernel stack\n", stack_base_page, stack_limit_page - 1);
}


int frame_alloc(int owner_pid) {
	TracePrintf(0, "THIS IS THE FRAME ALLOC FUNCTION _____________________________________???????????????\n");

	TracePrintf(0, "THIS IS THE PID NUMBER THAT WE WERE GIVEN -- %d\n", owner_pid);

	for (int i = 0; i < total_frames; i++) {
		if (frame_table[i].in_use == 0) {
			frame_table[i].in_use = 1;
           		frame_table[i].owner_pid = owner_pid;
			frame_table[i].refcount = 1;
			TracePrintf(0, "THIS IS OUR RETURN VALUE -- %d\n", i);
            		return i;
        }
    }

    printf("ERROR: Out of physical frames!\n");
    return ERROR;
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
