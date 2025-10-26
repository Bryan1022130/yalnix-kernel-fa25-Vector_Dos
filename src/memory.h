#pragma once
extern unsigned int total_physical_frames;

typedef struct Frame {
    int pfn;          // frame number
    int in_use;       // 0 = free, 1 = used
    int owner_pid;    // -1 for kernel
    int refcount;
} Frame;

// Core memory-management functions
void frames_init(unsigned int pmem_size);
int  frame_alloc(int owner_pid);
void frame_free(int pfn);
int  SetKernelBrk(void *addr);
