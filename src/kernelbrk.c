#include <hardware.h>
#include <ylib.h>

#define ENABLED 1 
#define DISABLED 0

//Global Variables to check if virtual memory is set up or not
//This will most likely be extern if KernelStart() is not in the scope of this file
short int vm_current_state = DISABLED;

//Keep track of the current state of brk
//It should be calculated with something like (_orig_kernel_brk_page * PAGESIZE);
//This will be handled in the KernelStart() function
void *current_brk;

int SetKernelBrk(void * addr){
	//Convert the address to uintptr_t to be able to use as a integer
	uintptr_t new_kbrk_addr = (uintptr_t)addr;
	uintptr_t old_kbrk = (uintptr_t)current_brk;

	/* SetKernelBrk operates differently depending on if virtual memory is enabled or not 
	 * Before Virtual Memory is enabled, it checks if and by how much the SetKernelBrk is being raised from the original kernel check point
	 */
	
	if(vm_current_state == DISABLED){
		//It can not be less then the original break point since this space is used for data
		if(new_kbrk_addr < old_kbrk){
			TracePrintf(0, "You can not shrink the heap!");
			return ERROR;
		}

	//Here check for more conditions that would be invalid, like if the new_kbrk_addr were to be greater than memory there is
	
	current_brk = new_kbrk_addr;

	} else if (vm_current_state == ENABLED){
	//SetKernelBrk functions as regular brk after Virtual Memory has been initalized

	uintptr_t original_brk_addr = (uintptr_t)(_orig_kernel_brk * PAGESIZE);
	if(new_kbrk_addr < original_brk_addr){
		TracePrintf(0, "Error! I will be writing into the data section!");
		return ERROR;
	}
	
	//Check if the requested new address space for the Kernel Heap Brk is valid
	if(new_kbrk_addr >= KERNEL_STACK_BASE){
		TracePrintf(0, "Your request will dip in kernel stack space error!\n");
		return ERROR;
	}

	//1.We can now call a function that will set up the page table entries for the new allocated space
	//2.We have to calculate the page-algned memory space to be able to loop
	//3.We would have to loop from the start of the old break pointer to the new one (By page size and not actual address value)
	//4.Try to allocate a new space in physical memory to map the virtual address to
	//5.On each iteration we need to set up the table entries with correct permissions and information for virtual memory
	
	//6.Perform other forms of error checking related to the addr passed in 
	

	if(new_kbrk_addr < old_kbrk){
		//7.Have logic here to be able to unmap pages and free up memory that was just being used
	}

	//Return 0 on success!
	return 0;
	}	
}





