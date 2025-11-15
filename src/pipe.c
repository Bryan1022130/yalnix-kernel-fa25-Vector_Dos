#include <string.h>
#include "pipe.h"
#include "process.h"
#include "Queue.h"

Pipe pipe_table[MAX_PIPES];

// Making Pipe table, called in kernelstart()
// resets pipes
void PipeTableInit(void) {
    memset(pipe_table, 0, sizeof(pipe_table));
}

// Find and return pointer to pipe by ID
Pipe *get_pipe(int id) {
    // if pipe exists, AND and its id is less than MAX_PIPES AND Pipe id not in use

    if (id < 0 || id >= MAX_PIPES || !pipe_table[id].in_use)
        // return error how can the pipe exist and its id not be in use?
        return NULL;

    return &pipe_table[id];
}


// called to free pipe entry so we can reuse the slot in the Pipe table
void PipeFree(int id) {
    if (id < 0 || id >= MAX_PIPES)
        return;

    Pipe *p = &pipe_table[id];
    p->in_use = 0;
    p->count = p->head = p->tail = 0;
    if (p->waiting_readers) free(p->waiting_readers);
    if (p->waiting_writers) free(p->waiting_writers);
    memset(p->buffer, 0, PIPE_BUFFER_LEN);
}


// returns the number of bytes left in pipe buffer
// used to check if writer can write more data without overflowing
int space_available(Pipe *p) {
    return PIPE_BUFFER_LEN - p->count;
}

// returns number of byes currently stored in the pipes buffer
// to check if theres data to read before blocking the process
int bytes_available(Pipe *p) {
    return p->count;
}

// handels wrapping logic (moves tail around)
// returns number of bytes actally written
int PipeWriteBytes(Pipe *p, const char *src, int len) {
    int written = 0;
    while (written < len && p->count < PIPE_BUFFER_LEN) {
        p->buffer[p->tail] = src[written];
        p->tail = (p->tail + 1) % PIPE_BUFFER_LEN;
        p->count++;
        written++;
    }
    return written;
}


//handles warpping logic (moves head around)
// returns number of bytes atually read
int PipeReadBytes(Pipe *p, char *dst, int len) {
    int read = 0;
    while (read < len && p->count > 0) {
        dst[read] = p->buffer[p->head];
        p->head = (p->head + 1) % PIPE_BUFFER_LEN;
        p->count--;
        read++;
    }
    return read;
}
