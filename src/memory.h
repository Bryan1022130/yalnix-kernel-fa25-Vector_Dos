#pragma once

//Header files from yalnix_framework && libc library
#include <sys/types.h> //For u_long
#include <load_info.h> //The struct for load_info
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL

//Our Header File
#include "process.h" //API for process block control
	
//KernelStart.c function declarations
int  SetKernelBrk(void *addr);
void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt);

//Frame logic 
int create_sframes(PCB *free_proc, unsigned char *track, int track_size);
void free_sframes(PCB *free_proc, unsigned char *track, int track_size);

//Physical memory logic
void init_frames(unsigned char *track, int track_size);
int find_frame(unsigned char *track, int track_size);
void frame_alloc(unsigned char *track, int frame_number);
void frame_free(unsigned char *track, int frame_number);

//Region Space logic
void SetupRegion0(pte_t *kernel_page_table);
void SetupRegion1(pte_t *user_page_table);

//Load Program function
int LoadProgram(char *name, char *args[], PCB *proc);
