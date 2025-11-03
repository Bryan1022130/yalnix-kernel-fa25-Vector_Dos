/*
 * ==>> This is a TEMPLATE for how to write your own LoadProgram function.
 * ==>> Places where you must change this file to work with your kernel are
 * ==>> marked with "==>>".  You must replace these lines with your own code.
 * ==>> You might also want to save the original annotations as comments.
 */

#include <fcntl.h>
#include <unistd.h>
#include <ykernel.h>
#include <load_info.h>

/*
 * ==>> #include anything you need for your kernel here
 */

//Header files from yalnix_framework && libc library
#include <sys/types.h> //For u_long
#include <ctype.h> // <----- NOT USED RIGHT NOW ----->
#include <load_info.h> //The struct for load_info
#include <ykernel.h> // Macro for ERROR, SUCCESS, KILL
#include <hardware.h> // Macro for Kernel Stack, PAGESIZE, ...
#include <yalnix.h> // Macro for MAX_PROCS, SYSCALL VALUES, extern variables kernel text: kernel page: kernel text
#include <ylib.h> // Function declarations for many libc functions, Macro for NULL
#include <yuser.h> //Function declarations for syscalls for our kernel like Fork() && TtyPrintf()
#include <sys/mman.h> // For PROT_WRITE | PROT_READ | PROT_EXEC

//Our Header Files
#include "Queue.h" //API calls for our queue data structure
#include "trap.h" //API for trap handling and initializing the Interrupt Vector Table
#include "memory.h" //API for Frame tracking in our program
#include "process.h" //API for process block control

#define VALID 1
#define INVALID 0

extern PCB *current_process;
extern unsigned char *track_global;
extern unsigned long int frame_count;

/*
 *  Load a program into an existing address space.  The program comes from
 *  the Linux file named "name", and its arguments come from the array at
 *  "args", which is in standard argv format.  The argument "proc" points
 *  to the process or PCB structure for the process into which the program
 *  is to be loaded.
 */

/*
 * ==>> Declare the argument "proc" to be a pointer to the PCB of
 * ==>> the current process.
 * Status: Done {Set it to PCB *proc}
 */

int
LoadProgram(char *name, char *args[], PCB *proc)

{
  int fd;
  int (*entry)();
  struct load_info li;
  int i;
  char *cp;
  char **cpp;
  char *cp2;
  int argcount;
  int size;
  int text_pg1;
  int data_pg1;
  int data_npg;
  int stack_npg;
  long segment_size;
  char *argbuf;


  /*
   * Open the executable file
   */
  if ((fd = open(name, O_RDONLY)) < 0) {
    TracePrintf(0, "LoadProgram: can't open file '%s'\n", name);
    return ERROR;
  }

  if (LoadInfo(fd, &li) != LI_NO_ERROR) {
    TracePrintf(0, "LoadProgram: '%s' not in Yalnix format\n", name);
    close(fd);
    return (-1);
  }

  if (li.entry < VMEM_1_BASE) {
    TracePrintf(0, "LoadProgram: '%s' not linked for Yalnix\n", name);
    close(fd);
    return ERROR;
  }

  /*
   * Figure out in what region 1 page the different program sections
   * start and end
   */
  text_pg1 = (li.t_vaddr - VMEM_1_BASE) >> PAGESHIFT; //The start of the text section
  data_pg1 = (li.id_vaddr - VMEM_1_BASE) >> PAGESHIFT; //The start of the data section
  data_npg = li.id_npg + li.ud_npg; // Used with data_pg1 for end of user data / heap

  /*
   *  Figure out how many bytes are needed to hold the arguments on
   *  the new stack that we are building.  Also count the number of
   *  arguments, to become the argc that the new "main" gets called with.
   */
  size = 0;
  for (i = 0; args[i] != NULL; i++) {
    TracePrintf(3, "counting arg %d = '%s'\n", i, args[i]);
    size += strlen(args[i]) + 1;
  }
  argcount = i;

  TracePrintf(2, "LoadProgram: argsize %d, argcount %d\n", size, argcount);

  /*
   *  The arguments will get copied starting at "cp", and the argv
   *  pointers to the arguments (and the argc value) will get built
   *  starting at "cpp".  The value for "cpp" is computed by subtracting
   *  off space for the number of arguments (plus 3, for the argc value,
   *  a NULL pointer terminating the argv pointers, and a NULL pointer
   *  terminating the envp pointers) times the size of each,
   *  and then rounding the value *down* to a double-word boundary.
   */
  cp = ((char *)VMEM_1_LIMIT) - size;

  cpp = (char **)
    (((int)cp -
      ((argcount + 3 + POST_ARGV_NULL_SPACE) *sizeof (void *)))
     & ~7);

  /*
   * Compute the new stack pointer, leaving INITIAL_STACK_FRAME_SIZE bytes
   * reserved above the stack pointer, before the arguments.
   */
  cp2 = (caddr_t)cpp - INITIAL_STACK_FRAME_SIZE;

  TracePrintf(1, "prog_size %d, text %d data %d bss %d pages\n",
              li.t_npg + data_npg, li.t_npg, li.id_npg, li.ud_npg);


  /*
   * Compute how many pages we need for the stack */
  stack_npg = (VMEM_1_LIMIT - DOWN_TO_PAGE(cp2)) >> PAGESHIFT;

  TracePrintf(1, "LoadProgram: heap_size %d, stack_size %d\n",
              li.t_npg + data_npg, stack_npg);


  /* leave at least one page between heap and stack */
  if (stack_npg + data_pg1 + data_npg >= MAX_PT_LEN) {
    close(fd);
    return ERROR;
  }

  /*
   * This completes all the checks before we proceed to actually load
   * the new program.  From this point on, we are committed to either
   * loading succesfully or killing the process.
   */

  /*
   * Set the new stack pointer value in the process's UserContext
   */

  /*
   * ==>> (rewrite the line below to match your actual data structure)
   * ==>> proc->uc.sp = cp2;
   * Status: Done
   */

  proc->curr_uc.sp = cp2;

  /*
   * Now save the arguments in a separate buffer in region 0, since
   * we are about to blow away all of region 1.
   */
  cp2 = argbuf = (char *)malloc(size);

  /*
   * ==>> You should perhaps check that malloc returned valid space
   * Status: Done
   */

  //Malloc check
  if(cp2 == NULL || argbuf == NULL){
          //Just to check the return address from the malloc called
          TracePrintf(0, "This is malloc value of cp2 --> %p and this for argbug --> %p\n",(void *)cp2, (void *)argbuf);
          TracePrintf(0, "Malloc failed when calling it for cp2 or argbuf");
          return ERROR;
  }


  for (i = 0; args[i] != NULL; i++) {
    TracePrintf(3, "saving arg %d = '%s'\n", i, args[i]);
    strcpy(cp2, args[i]);
    cp2 += strlen(cp2) + 1;
  }

  /*
   * Set up the page tables for the process so that we can read the
   * program into memory.  Get the right number of physical pages
   * allocated, and set them all to writable.
   */

  /* ==>> Throw away the old region 1 virtual address space by
   * ==>> curent process by walking through the R1 page table and,
   * ==>> for every valid page, free the pfn and mark the page invalid.
   */

  //Loop from text_pg1 to MAX_PT_LEN - 1
  unsigned int PAGE_CYCLES = MAX_PT_LEN;

  //Get the byte address of the start of the page table for region 1
  //This is stored in our PCB and points to the virtual space
  //but physical memory is also change since its mapped
  
  pte_t *PT = (pte_t *)proc->AddressSpace;

  for(unsigned int i = 0; i < PAGE_CYCLES; i++){
          if(PT[i].valid == VALID){

                //free the physical frame
		if(PT[i].pfn > 0){
                	frame_free(track_global,PT[i].pfn);
		}

                //Set the page as invalid
                PT[i].pfn = 0;
                PT[i].valid = INVALID;
                PT[i].prot = 0;
          }
  }

  /*
   * ==>> Then, build up the new region1.
   * ==>> (See the LoadProgram diagram in the manual.)
   */

  /*
   * ==>> First, text. Allocate "li.t_npg" physical pages and map them starting at
   * ==>> the "text_pg1" page in region 1 address space.
   * ==>> These pages should be marked valid, with a protection of
   * ==>> (PROT_READ | PROT_WRITE).
   */

  TracePrintf(0, "We are going to set up region 1 space now\n");
  // First set up the text regions
  unsigned int text_end = text_pg1 + li.t_npg;

  for(unsigned int j = text_pg1; j <= text_end; j++){
        //Grab a physical frame; based on the manual
        unsigned int pfn_grab = find_frame(track_global, frame_count);
	TracePrintf(0, "This is the pfn we got for text -- > %d", pfn_grab);
        if(pfn_grab == ERROR){
                TracePrintf(0, "Error when allocating a frame !\n");
                return ERROR;
        }

        //Set up the permission and mark the pages as valid
        PT[j].pfn = pfn_grab;
        PT[j].valid = VALID;
        PT[j].prot = (PROT_READ | PROT_WRITE);
  }

  /*
   * ==>> Then, data. Allocate "data_npg" physical pages and map them starting at
   * ==>> the  "data_pg1" in region 1 address space.
   * ==>> These pages should be marked valid, with a protection of
   * ==>> (PROT_READ | PROT_WRITE).
   */
  unsigned int data_end = data_pg1 + data_npg;

  for(unsigned int u = data_pg1; u <= data_end; u++){
	  unsigned int pfn_grab = find_frame(track_global, frame_count);
	TracePrintf(0, "This is the pfn we got for data -- > %d", pfn_grab);
          if(pfn_grab == ERROR){
                  TracePrintf(0, "Error when allocating a frame !\n");
                  return ERROR;
          }

          //Set up the permission and makr the pages as invalid
          PT[u].pfn = pfn_grab;
          PT[u].valid = VALID;
          PT[u].prot = (PROT_READ | PROT_WRITE);
  }

  /*
   * ==>> Then, stack. Allocate "stack_npg" physical pages and map them to the top
   * ==>> of the region 1 virtual address space.
   * ==>> These pages should be marked valid, with a
   * ==>> protection of (PROT_READ | PROT_WRITE).
   */
  unsigned int stack_start = MAX_PT_LEN - stack_npg;

  for(unsigned int t = stack_start; t <= PAGE_CYCLES; t++){
          //Allocated a physical frame to store in region 1
          unsigned int pfn_grab = find_frame(track_global, frame_count);
	TracePrintf(0, "This is the pfn we got for stack -- > %d", pfn_grab);
          if(pfn_grab == ERROR){
                  TracePrintf(0, "Error when allocating a frame !\n");
                  return ERROR;
          }

          PT[t].pfn = pfn_grab;
          PT[t].valid = VALID;
          PT[t].prot = (PROT_READ | PROT_WRITE);
  }

  /*
   * ==>> (Finally, make sure that there are no stale region1 mappings left in the TLB!)
   */

  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

  /*
   * All pages for the new address space are now in the page table.
   */

  /*
   * Read the text from the file into memory.
   */
  lseek(fd, li.t_faddr, SEEK_SET);
  segment_size = li.t_npg << PAGESHIFT;
  if (read(fd, (void *) li.t_vaddr, segment_size) != segment_size) {
    close(fd);
    return KILL;   // see ykernel.h
  }

  /*
   * Read the data from the file into memory.
   */
  lseek(fd, li.id_faddr, 0);
  segment_size = li.id_npg << PAGESHIFT;

  if (read(fd, (void *) li.id_vaddr, segment_size) != segment_size) {
    close(fd);
    return KILL;
  }


  close(fd);                    /* we've read it all now */


  /*
   * ==>> Above, you mapped the text pages as writable, so this code could write
   * ==>> the new text there.
   *
   * ==>> But now, you need to change the protections so that the machine can execute
   * ==>> the text.
   *
   * ==>> For each text page in region1, change the protection to (PROT_READ | PROT_EXEC).
   * ==>> If any of these page table entries is also in the TLB,
   * ==>> you will need to flush the old mapping.
   */

  for(unsigned int j = text_pg1; j < text_end; j++){
        //Change the permission to excutable for the text pages
        PT[j].prot = (PROT_READ | PROT_EXEC);
  }

  //Flush now in case any of these pages were in the TLB
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

  /*
   * Zero out the uninitialized data area
   * I added the cast of (void *) for li.id_end because it gave me warnings that were annyoing
   */
  bzero((void *)li.id_end, li.ud_end - li.id_end);

  /*
   * Set the entry point in the process's UserContext
   */

  /*
   * ==>> (rewrite the line below to match your actual data structure)
   * ==>> proc->uc.pc = (caddr_t) li.entry;
   */
  proc->curr_uc.pc = (caddr_t)li.entry;

  /*
   * Now, finally, build the argument list on the new stack.
   */


  memset(cpp, 0x00, VMEM_1_LIMIT - ((int) cpp));

  *cpp++ = (char *)argcount;            /* the first value at cpp is argc */
  cp2 = argbuf;
  for (i = 0; i < argcount; i++) {      /* copy each argument and set argv */
    *cpp++ = cp;
    strcpy(cp, cp2);
    cp += strlen(cp) + 1;
    cp2 += strlen(cp2) + 1;
  }
  free(argbuf);
  *cpp++ = NULL;                        /* the last argv is a NULL pointer */
  *cpp++ = NULL;                        /* a NULL pointer for an empty envp */

  return SUCCESS;
}
