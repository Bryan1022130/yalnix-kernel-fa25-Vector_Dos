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
	
typedef struct Frame {
    int pfn;          // frame number
    int in_use;       // 0 = free, 1 = used
    int owner_pid;   
    int refcount;
} Frame;

// Core memory-management functions
void frames_init(unsigned int pmem_size);
int  frame_alloc(int owner_pid);
void frame_free(int pfn)

//KernelStart.c function declarations
int  SetKernelBrk(void *addr);
void DoIdle(void);
PCB *createInit(void);
void InitPcbTable(void);
PCB *pcb_alloc(void);
int pcb_free(int pid);
void init_proc_create(void);
void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt);
void init(void);

//Load Program function
int LoadProgram(char *name, char *args[], PCB *proc);
