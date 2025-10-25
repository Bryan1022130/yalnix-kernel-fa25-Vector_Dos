#include <hardware.h>
#include <stdint.h>
#include <ylib.h>
#include "memory.h"
#include <sys/mman.h>

#define ENABLED 1 
#define DISABLED 0

//Global Variables to check if virtual memory is set up or not
//This will most likely be extern if KernelStart() is not in the scope of this file
short int vm_current_state = DISABLED;

//Keep track of the current state of brk
//It should be calculated with something like (_orig_kernel_brk_page * PAGESIZE);
//This will be handled in the KernelStart() function
void *current_brk;

extern pte_t kernel_page_table[];  // defined in kernelstart.c
extern unsigned long _orig_kernel_brk;        // byte address
extern unsigned long int frame_count;   // set in KernelStart; used for phys limit check

int SetKernelBrk(void * addr){
	//Convert the address to uintptr_t to be able to use as a integer
	uintptr_t new_kbrk_addr = (uintptr_t)addr;
	uintptr_t old_kbrk = (uintptr_t)current_brk;

	/* SetKernelBrk operates differently depending on if virtual memory is enabled or not 
	 * Before Virtual Memory is enabled, it checks if and by how much the SetKernelBrk is being raised from the original kernel check point
	 */
	
	TracePrintf(1, "[SetKernelBrk] old=%p new=%p state=%s\n",
                current_brk, addr,
                (vm_current_state == ENABLED ? "ENABLED" : "DISABLED"));

	if(vm_current_state == DISABLED){
		//It can not be less then the original break point since this space is used for data
		if(new_kbrk_addr < old_kbrk){
			TracePrintf(0, "You can not shrink the heap!");
			return ERROR;
		}

		//Here check for more conditions that would be invalid, like if the new_kbrk_addr were to be greater than memory there is
		// --- address validity checks before VM is enabled ---
		uintptr_t heap_start = (uintptr_t)_orig_kernel_brk;
		uintptr_t heap_end_limit = (uintptr_t)KERNEL_STACK_BASE;

		// 1. can't shrink below data/heap start
		if (new_kbrk_addr < heap_start) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p is below kernel heap start (%p)\n",
		                (void*)new_kbrk_addr, (void*)heap_start);
		    return ERROR;
		}

		// 2. can't grow into kernel stack region
		if (new_kbrk_addr >= heap_end_limit) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p would overlap kernel stack (%p)\n",
		                (void*)new_kbrk_addr, (void*)heap_end_limit);
		    return ERROR;
		}

		// 3. optionally, if you’ve stored total physical memory
		extern unsigned long frame_count;  // set in KernelStart
		uintptr_t phys_limit = frame_count * PAGESIZE;
		if (new_kbrk_addr >= phys_limit) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p exceeds physical memory limit (%p)\n",
		                (void*)new_kbrk_addr, (void*)phys_limit);
		    return ERROR;
		}

		// 4. warn if not page aligned
		if ((new_kbrk_addr & PAGEOFFSET) != 0) {
		    TracePrintf(1, "[SetKernelBrk] Warning: unaligned address %p; rounding internally.\n",
		                (void*)new_kbrk_addr);
		}

		current_brk = (void *)new_kbrk_addr;
		TracePrintf(1, "[SetKernelBrk] Updated current_brk (no VM): %p\n", current_brk);
        	return 0;
	}
	else if (vm_current_state == ENABLED){
		//SetKernelBrk functions as regular brk after Virtual Memory has been initalized
		
		//Stores the byte address of the kernel break
		uintptr_t original_brk_addr = (uintptr_t)_orig_kernel_brk;

		if(new_kbrk_addr < original_brk_addr){
			TracePrintf(0, "Error! I will be writing into the data section!");
			return ERROR;
		}
		
		//Check if the requested new address space for the Kernel Heap Brk is valid
		// --- address validity checks ---
		uintptr_t heap_start = (uintptr_t)_orig_kernel_brk;
		uintptr_t heap_end_limit = (uintptr_t)KERNEL_STACK_BASE;

		// 1. must not go below heap start (data section)
		if (new_kbrk_addr < heap_start) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p is below kernel heap start (%p)\n",
		                (void*)new_kbrk_addr, (void*)heap_start);
		    return ERROR;
		}

		// 2. must not grow into kernel stack
		if (new_kbrk_addr >= heap_end_limit) {
		    TracePrintf(0, "[SetKernelBrk] Error: address %p overlaps kernel stack base (%p)\n",
		                (void*)new_kbrk_addr, (void*)heap_end_limit);
		    return ERROR;
		}

		// 3. warn if not page aligned (not fatal, but we’ll round internally)
		if ((new_kbrk_addr & PAGEOFFSET) != 0) {
		    TracePrintf(1, "[SetKernelBrk] Warning: unaligned address %p; rounding to page boundary.\n",
		                (void*)new_kbrk_addr);
		}

		//1.We can now call a function that will set up the page table entries for the new allocated space
		//2.We have to calculate the page-algned memory space to be able to loop
		//3.We would have to loop from the start of the old break pointer to the new one (By page size and not actual address value)
		//4.Try to allocate a new space in physical memory to map the virtual address to
		//5.On each iteration we need to set up the table entries with correct permissions and information for virtual memory
		
		//6.Perform other forms of error checking related to the addr passed in 

		// Step 1: Align old/new break values
		// ---------------------------------------------------------
		uintptr_t grow_start = UP_TO_PAGE(old_kbrk);        // first new page if growing
		uintptr_t grow_end   = UP_TO_PAGE(new_kbrk_addr);   // one past last page
		// If grow_end > grow_start → we’re expanding the heap
		// If grow_end < grow_start → we’re shrinking

		// Step 2: Growing the heap (allocate frames)
		if (grow_end > grow_start) {
		        TracePrintf(1, "[SetKernelBrk] Growing kernel heap...\n");
		        uintptr_t mapped_until = grow_start;   // track how far we’ve gotten

		        for (uintptr_t vaddr = grow_start; vaddr < grow_end; vaddr += PAGESIZE) {
		                int vpn = (int)(vaddr >> PAGESHIFT);

		                // skip if already valid 
		                if (kernel_page_table[vpn].valid) {
		                        mapped_until = vaddr + PAGESIZE;
		                        continue;
		                }

		                int pfn = frame_alloc(-1);   // -1 marks kernel ownership
		                if (pfn < 0) {
		                        // roll back anything we just mapped
		                        for (uintptr_t rv = grow_start; rv < mapped_until; rv += PAGESIZE) {
		                                int rvpn = (int)(rv >> PAGESHIFT);
		                                if (kernel_page_table[rvpn].valid) {
		                                        frame_free(kernel_page_table[rvpn].pfn);
		                                        kernel_page_table[rvpn].valid = 0;
		                                        kernel_page_table[rvpn].prot  = 0;
		                                        kernel_page_table[rvpn].pfn   = 0;
		                                }
		                        }
		                        TracePrintf(0, "[SetKernelBrk] Out of frames while growing!\n");
		                        return ERROR;
		                }

		                kernel_page_table[vpn].pfn   = pfn;
		                kernel_page_table[vpn].prot  = PROT_READ | PROT_WRITE;
		                kernel_page_table[vpn].valid = 1;

		                mapped_until = vaddr + PAGESIZE;
		        }

		        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
		}
		else if (grow_end < grow_start) {
		        TracePrintf(1, "[SetKernelBrk] Shrinking kernel heap...\n");
		        for (uintptr_t vaddr = grow_end; vaddr < grow_start; vaddr += PAGESIZE) {
		                int vpn = (int)(vaddr >> PAGESHIFT);
		                if (kernel_page_table[vpn].valid) {
		                        frame_free(kernel_page_table[vpn].pfn);
		                        kernel_page_table[vpn].valid = 0;
		                        kernel_page_table[vpn].prot  = 0;
		                        kernel_page_table[vpn].pfn   = 0;
		                }
		        }

		        // flush TLB for region 0
		        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
		}
		 // Step 4: Finalize and return
	        // ------------------------------------------------------------
	        current_brk = (void *)new_kbrk_addr;
	        TracePrintf(1, "[SetKernelBrk] Moved break to %p (VM enabled)\n", current_brk);
	        return 0;
	    }
	TracePrintf(0, "[SetKernelBrk] Unknown VM state!\n");
	return ERROR;
}




