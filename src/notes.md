# Boot:
In the first transition the kernel invoke a function called ==KernelStart()==. When the function returns then we can start running in user mode. 

# Kernel Heap

We will be given a kernel library that is provided. This will include things such as malloc(), there is Kernel version of brk that we can use to adjust the top of the heap. the function is called SetKernelBrk(). 

#### We will write this function ==SetKernelBrk()==

---
# Hardware

### All registers on the DCS 58 are 32 bits wide

There is general purpose registers that can be accessed by ether Kernel or User mode and these include: 
• PC, the program counter
• SP, the stack pointer, contains the virtual address of the top of the current process’s stack.  
• EBP, the frame pointer (aka base pointer contains the virtual address of the current process’s stack frame
• R0 through R7, the eight general registers, used as accumulators or otherwise to hold temporary values during  
a computation.

---
### Privileged Registers

These are only register that can be accessed by kernel or when in kernel mode.

Most of the privileged registers are both writable and readable, except for the REG_TLB_FLUSH register, which is write-only.  

The hardware provides two privileged instructions for reading and writing these privileged machine registers:  

``` jsx

• void WriteRegister(int which, unsigned int value)  

Write value into the privileged machine register designated by which.  

• unsigned int ReadRegister(int which)  

Read the register specified by which and return its current value.  

```
##### The file hardware.h defines the symbolic names for these constants. 

When calling `WriteRegister`, you must **cast** other data types (like addresses) to `unsigned int`.

You must also **cast** the value returned by `ReadRegister` to the desired type if it needs to be interpreted as something other than an `unsigned'

---
# Memory Subsystem

hardware.h defines a number of constants and macros to make dealing with addresses and page numbers easier:  

• PAGESIZE: The size in bytes of each physical memory frame and virtual memory page.  

• PAGEOFFSET: A bit mask that can be used to extract the byte offset with a page

• PAGEMASK: A bit mask that can be used to extract the beginning address of a page

• PAGESHIFT: The log base 2 of PAGESIZE, which is thus also the number of bits in the offset in a physical or  
virtual address.

• UP TO PAGE(addr): A C preprocessor macro that rounds address addr up to the 0th address in the next  
highest page—in other words, the next highest page boundary. 

• DOWN TO PAGE(addr): A C preprocessor macro that rounds address addr down to the next lowest page  
boundary (the 0th address of the next lowest page).

PMEM BASE: The physical memory address of the first byte of physical memory in the machine.

• The total size (number of bytes) of physical memory in the computer is determined by how much RAM is  
installed on the machine. At boot time, the computer’s firmware tests the physical memory and determines the  
amount of memory installed, and passes this value to the initialization procedure of the operating system kernel.

##### As is standard physical frames are equal size to virtual page memory

---

# Virtual Address Space 

By convention, Region 0 is used by the operating system kernel, and Region 1 is used by user processes executing on the operating system.

Macros to control virtual space:

VMEM_0_BASE: The lowest virtual address of Region 0

VNEM_0_LIMIT: The limit virtual address of Region 0(first byte above the end of Region 0)

VMEM_0_SIZE is the size of Region 0

The beginning virtual address of Region 1 is defined as VMEM_1_BASE

The limit virtual address (first byte above the region) is VMEM_1_LIMIT

Size is VMEM_1_SIZE

---
# Page Tables

The kernel allocates page tables in memory wherever it wants to, and it tells the MMU in the hardware where to  
find these page tables through the following privileged registers:

• REG PTBR0: Contains the virtual memory base address of the page table for Region 0 of virtual memory.  

• REG PTLR0: Contains the number of entries in the page table for virtual memory Region 0, i.e., the number of  
virtual pages in Region 0.  

• REG PTBR1: Contains the virtual memory base address of the page table for Region 1 of virtual memory.  

• REG PTLR1: Contains the number of entries the page table for virtual memory Region 1, i.e., the number of  
virtual pages in Region 1

A 32-bit PTE typically contains the following key fields:

Control Bits:
	This is 4 bits wide and is composed of a valid bit and 3 permission bits
	
Unused bits:
	Unused space in the PTE

Page Frame Number:
	This is the most significant part of the physical memory address where the page is actually located.

---

# **Initializing Virtual Memory**

On the DCS58, virtual memory cannot be disabled after it is enabled (that is, without rebooting the  machine).

To enable virtual memory without crashing the kernel, the initial virtual address space for the kernel must perfectly 
mirror the physical address space it's already using.
