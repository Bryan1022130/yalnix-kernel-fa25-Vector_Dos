//Header files from yalnix_framework && libc library
#include <sys/types.h> //For u_long
#include <load_info.h> //The struct for load_info
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL
#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC
#include <stdint.h>

//Our Header files
#include "memory.h"

extern pte_t *kernel_page_table;
extern int vm_enabled;
extern unsigned int frame_count;
extern void *current_kernel_brk;
extern unsigned char *track_global;

#define FALSE 0
#define TRUE 1

/* ==============================================================================================
 * SetKernelBrk Function Logic
 * ==============================================================================================
 */

int SetKernelBrk(void * addr){
        TracePrintf(1, "We are in the function SetKernelBrk\n");

        //Convert the address to uintptr_t to be able to use as a integer
        uintptr_t new_kbrk_addr = (uintptr_t)addr;
        uintptr_t old_kbrk = (uintptr_t)current_kernel_brk;

        /* SetKernelBrk operates differently depending on if virtual memory is enabled or not
         * Before Virtual Memory is enabled, it checks if and by how much the SetKernelBrk is being raised from the original kernel check point
         */

        if(vm_enabled == FALSE){
                TracePrintf(1, "THIS IS CALLED WHEN VIRTUAL MEMORY IS NOT ENABLED\n");

                uintptr_t original_brk_addr = (uintptr_t)_orig_kernel_brk_page * PAGESIZE;

                //It can not be less then the current break point since this space is used for data
                if(new_kbrk_addr < old_kbrk){
                        TracePrintf(0, "You can not shrink the heap!\n");
                        return ERROR;
                }

                uintptr_t heap_start = (uintptr_t)_orig_kernel_brk_page * PAGESIZE;
                uintptr_t heap_end_limit = (uintptr_t)KERNEL_STACK_BASE;

                // can't shrink below data/heap start
                if (new_kbrk_addr < heap_start) {
                    TracePrintf(0, "[SetKernelBrk] Error: address %p is below kernel heap start (%p)\n", (void*)new_kbrk_addr, (void*)heap_start);
                    return ERROR;
                }

                // can't grow into kernel stack region
                if (new_kbrk_addr >= heap_end_limit) {
                    TracePrintf(0, "[SetKernelBrk] Error: address %p would overlap kernel stack (%p)\n", (void*)new_kbrk_addr, (void*)heap_end_limit);
                    return ERROR;
                }

                uintptr_t phys_limit = frame_count * PAGESIZE;
                if (new_kbrk_addr >= phys_limit) {
                    TracePrintf(0, "[SetKernelBrk] Error: address %p exceeds physical memory limit (%p)\n",
                                (void*)new_kbrk_addr, (void*)phys_limit);
                    return ERROR;
                }

                current_kernel_brk = (void *)new_kbrk_addr;

                TracePrintf(1, "[SetKernelBrk] Updated current_brk (no VM): %p\n", current_kernel_brk);
                return 0;
        }
        else if (vm_enabled == TRUE){
                TracePrintf(1, "VIRTUAL MEMORY HAS BEEN ENABLED \n");

                //SetKernelBrk functions as regular brk after Virtual Memory has been initialized
                //Stores the byte address of the kernel break

                //Check if the requested new address space for the Kernel Heap Brk is valid

                uintptr_t heap_start = (uintptr_t)_orig_kernel_brk_page * PAGESIZE;
                uintptr_t heap_end_limit = (uintptr_t)KERNEL_STACK_BASE;


                TracePrintf(1, "[SetKernelBrk-VM] Comparing new_brk: %p against heap_start: %p\n",
                    (void*)new_kbrk_addr, (void*)heap_start);


                if (new_kbrk_addr < heap_start) {
                    TracePrintf(0, "[SetKernelBrk] Error: address %p is below kernel heap start (%p)\n", (void*)new_kbrk_addr, (void*)heap_start);
                    return ERROR;
                }

                if (new_kbrk_addr >= heap_end_limit) {
                    TracePrintf(0, "[SetKernelBrk] Error: address %p overlaps kernel stack base (%p)\n", (void*)new_kbrk_addr, (void*)heap_end_limit);
                    return ERROR;
                }

                uintptr_t grow_start = UP_TO_PAGE(old_kbrk);        
                uintptr_t grow_end   = UP_TO_PAGE(new_kbrk_addr); 

                // Step 2: Growing the heap (allocate frames)
                        // --- Grow the kernel heap ---
        if (grow_end > grow_start) {
            TracePrintf(1, "[SetKernelBrk] Growing kernel heap...\n");
            for (uintptr_t vaddr = grow_start; vaddr < grow_end; vaddr += PAGESIZE) {
                int vpn = (int)(vaddr >> PAGESHIFT);
                if (kernel_page_table[vpn].valid) continue;

                int pfn = find_frame(track_global, frame_count);
                if (pfn == ERROR) {
                    TracePrintf(0, "[SetKernelBrk] Out of physical frames!\n");
                    return ERROR;
                }

                frame_alloc(track_global, pfn);
                kernel_page_table[vpn].pfn = pfn;
                kernel_page_table[vpn].prot = PROT_READ | PROT_WRITE;
                kernel_page_table[vpn].valid = TRUE;
            }
            WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
        }

        // --- Shrink the kernel heap ---
        else if (grow_end < grow_start) {
            TracePrintf(1, "[SetKernelBrk] Shrinking kernel heap...\n");
            for (uintptr_t vaddr = grow_end; vaddr < grow_start; vaddr += PAGESIZE) {
                int vpn = (int)(vaddr >> PAGESHIFT);
                if (kernel_page_table[vpn].valid) {
                    frame_free(track_global, kernel_page_table[vpn].pfn);
                    kernel_page_table[vpn].valid = FALSE;
                    kernel_page_table[vpn].prot = 0;
                    kernel_page_table[vpn].pfn = 0;
                }
            }
            WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
        }

        // --- Finalize ---
        current_kernel_brk = (void *)new_kbrk_addr;
        TracePrintf(1, "[SetKernelBrk] Moved break to %p (VM enabled)\n", current_kernel_brk);
        return 0;
    }

    TracePrintf(0, "[SetKernelBrk] Unknown VM state!\n");
    return ERROR;
}
