#pragma once
#include "process.h"
#include "memory.h"

/*
 * System Call Interface
 */

int KernelGetPid(void);
int KernelFork(void);
int KernelExec(char *filename, char *argv[]);
void KernelExit(int status);
int KernelWait(int *status_ptr);
int KernelBrk(void *addr);
int KernelDelay(int clock_ticks);
int KernelTtyRead(int tty_id, void *buf, int len);
int KernelTtyWrite(int tty_id, void *buf, int len);
int KernelReadSector(int sector, void *buf);
int KernelWriteSector(int sector, void *buf);
int KernelPipeInit(int *pipe_id_ptr);
int KernelPipeRead(int pipe_id, void *buf, int len);
int KernelPipeWrite(int pipe_id, void *buf, int len);
int KernelLockInit(int *lock_idp);
int KernelAcquire(int lock_id);
int KernelRelease(int lock_id);
int KernelCvarInit(int *cvar_idp);
int KernelCvarSignal(int cvar_id);
int KernelCvarBroadcast(int cvar_id);
int KernelCvarWait(int cvar_id, int lock_id);
int KernelReclaim(int lock_id);

//Helper functions 
void rollback_frames(int first_frame, int amount);
void data_copy(void *parent_pte, int cpfn);
