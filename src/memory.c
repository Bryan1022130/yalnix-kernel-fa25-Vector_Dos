//Our header files
#include "memory.h"
#include <stdio.h>
#include <yalnix.h>
#include <hardware.h> 
#include <ykernel.h> 

#define TEMP_KSTACK 125
#define V1_KSTACK 126
#define V2_KSTACK 127

#define IN_USE 1
#define UNUSED 0

//Mark all frames unused
//Note: We need to reserved space for the data section of the kernel since that dosen't change
//Note: We also need to alloc frames for the kernel stack
void init_frames(unsigned char *track, int track_size){

	for(int x = 0; x < track_size; x++){
		track[x] = UNUSED;
	}

	//Allocated stack frames for the kernel data section
	for(int y = 0; y < _orig_kernel_brk_page; y++){
		frame_alloc(track, y);
	}
	
	//For when we need to access the kernel stack temporarily
	frame_alloc(track, TEMP_KSTACK);
	
	//Alloc frames for the kernel stack
	frame_alloc(track, V1_KSTACK);
	frame_alloc(track, V2_KSTACK);
}

//Loop through the current buffer and just return the first frame available
int find_frame(unsigned char *track, int track_size){
	for(int z = 0; z < track_size; z++){
		if(track[z] == UNUSED){
			frame_alloc(track, z);
			return z;
		}
	}
	TracePrintf(0, "We did not find any free frames! Returning ERROR\n");
	return ERROR;
}

//Index into buffer and set as in use
void frame_alloc(unsigned char *track, int frame_number){
	TracePrintf(0, "Info --> This is the frame that is being alloced -> %d\n", frame_number);
	track[frame_number] = IN_USE;
}

void frame_free(unsigned char *track, int frame_number){
	TracePrintf(0, "Info --> This is the frame that is being freed -> %d\n", frame_number);
	track[frame_number] = UNUSED;
}
