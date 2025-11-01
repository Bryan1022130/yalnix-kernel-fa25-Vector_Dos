#include "memory.h"
#include <stdio.h>
#include <yalnix.h>
#include <hardware.h> 
#include <ykernel.h> 

//global variables for tracking frames
static Frame *frame_table = NULL;
int total_frames = 0;
extern void *current_kernel_brk;

#define USING 1
#define UNUSED 0
#define KERNEL -1

// Build the frame table representing all physical pages.
void frames_init(unsigned int pmem_size) {

    //Compute total_frames = pmem_size / PAGESIZE
    total_frames = pmem_size / PAGESIZE;

    frame_table = (Frame *)malloc(sizeof(Frame) * total_frames);
    if (frame_table == NULL) {
        TracePrintf(0, "frames_init: malloc failed for %d frames\n", total_frames);
        Halt();
    }
    
    //Initialize all frames as free first
    for (int i = 0; i < total_frames; i++) {
        frame_table[i].pfn = i;
	frame_table[i].in_use = UNUSED;
        frame_table[i].owner_pid = KERNEL;
        frame_table[i].refcount = 0;
    }
    
    // Step 3: Mark LOW MEMORY (frames 0 to _first_kernel_text_page-1) as used
    unsigned long first_kernel_page = (unsigned long)_first_kernel_text_page;
    
    for (unsigned long i = 0; i < first_kernel_page; i++) {
        if (i < total_frames) {
            frame_table[i].in_use = USING;
            frame_table[i].owner_pid = KERNEL;
            frame_table[i].refcount = 1;
        }
    }
    //Step 4
    unsigned long kernel_brk_page = (int)current_kernel_brk >> PAGESHIFT;
    TracePrintf(1, "Current kernel break is at page %lu\n", kernel_brk_page);

    for (unsigned long i = first_kernel_page; i < kernel_brk_page; i++) {
        if (i < total_frames) {
            frame_table[i].in_use = USING;
            frame_table[i].owner_pid = KERNEL;
            frame_table[i].refcount = 1;
        }
    }
    
    // Step 5: Mark kernel stack as used
    unsigned long stack_base_page = KERNEL_STACK_BASE >> PAGESHIFT;
    unsigned long stack_limit_page = KERNEL_STACK_LIMIT >> PAGESHIFT;
    
    for (unsigned long i = stack_base_page; i < stack_limit_page; i++) {
        if (i < total_frames) {
            frame_table[i].in_use = USING;
            frame_table[i].owner_pid = KERNEL;
            frame_table[i].refcount = 1;
        }
    }
    
    TracePrintf(1, "Frame table initialization done\n");
    TracePrintf(1, "  Marked frames 0-%lu as low memory (reserved)\n", first_kernel_page - 1);
    TracePrintf(1, "  Marked frames %lu-%lu as kernel text/data\n", first_kernel_page, kernel_brk_page - 1);
    TracePrintf(1, "  Marked frames %lu-%lu as kernel stack\n", stack_base_page, stack_limit_page - 1);
    TracePrintf(1, "Leaving the frame_init function\n\n");

}

// Find a free frame and mark it as owned by 'owner_pid'.
// Returns the physical frame number (PFN), or ERROR if full.
int frame_alloc(int owner_pid) {
	TracePrintf(0, "--->This is the pid number given into the function frame_alloc: %d\n", owner_pid);

	for (int i = 0; i < total_frames; i++) {
		if (frame_table[i].in_use == UNUSED) {
			frame_table[i].in_use = 1;
           		frame_table[i].owner_pid = owner_pid;
			frame_table[i].refcount = 1;
			TracePrintf(0, "###>This is the physical frame that was found: %d\n", i);
            		return i;
        }
    }

    printf("ERROR: Out of physical frames!\n");
    return ERROR;
}

// Decrement refcount and release the frame if no longer used.
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
        frame_table[pfn].in_use = UNUSED;
        frame_table[pfn].owner_pid = -1;
    }
}
