#include <stddef.h>
#include "memory.h"
#include "process.h"
#include "sched.h"
#include "traps.h"



void KernelStart(char *cmd_args[], unsigned int pmem_size, struct UserContext *uctx_init) {
    /*
     * 1. Initialize free-frame tracking from pmem_size.
	frames_init(pmem_size);
     * 2. Build Region 0 page table (identity map kernel memory).
	r0_init_and_map_kernel(pmem_size);
     * 3. Prepare kernel heap and SetKernelBrk().
	
     * 4. Install trap vector table.
	traps_init();
     * 5. Create first process (init).
	pid_t idle_pid = proc_alloc(r0_pt(), r0_ptlr());
	PCB *idle = proc_by_pid(idle_pid);
     * 6. Enable virtual memory.
     * 7.Ready queues and scheduling
	sched_make_runnable(idle);
	g_current = idle;
     * 8. Return into user mode (start init program).
	return;
     */
}
