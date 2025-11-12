#ifndef _PIPE_H_
#define _PIPE_H_

#include "Queue.h"
#include "ykernel.h"
#include "yalnix.h"

#define MAX_PIPES 64

typedef struct Pipe {
    char buffer[PIPE_BUFFER_LEN];
    int head; // where read indexes
    int tail; // where write indexes
    int count; // number of bytes stored
    int in_use; // 1 if in use and 0 if free

    Queue *waiting_readers;
    Queue *waiting_writers;
}Pipe;

extern Pipe pipe_table[MAX_PIPES];

// starting pipes
void PipeTableInit(void);
Pipe *get_pipe(int id);
void PipeFree(int id);

int PipeWriteBytes(Pipe *p, const char *src, int len);
int PipeReadBytes(Pipe *p, char *dst, int len);

int space_available(Pipe *p);
int bytes_available(Pipe *p);

#endif
