#include "cvar.h"
#include <stdlib.h>
#include <string.h>
#include "Queue.h"

Cvar *linked_cvars = NULL;
int cvar_count = 0;


// called in kernel start
void CvarTableInit(void) {
    linked_cvars = NULL;
    cvar_count = 0;
}

// for finidng specific cvar by ID
Cvar *find_cvar(int id) {
    Cvar *curr = linked_cvars;
    while (curr) {
        if (curr->id == id && curr->in_use)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

// for adding a new cvar to the list 
void add_cvar(Cvar *cv) {
    cv->next = linked_cvars;
    linked_cvars = cv;
}
