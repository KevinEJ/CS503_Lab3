/* vcreate.c - vcreate */

#include <xinu.h>

local	int newpid();

#define	roundew(x)	( (x+3)& ~0x3)

/*----------------------------------------------------------------------------
 *  vcreate  -  Creates a new XINU process. The process's heap will be private
 *  and reside in virtual memory.
 *----------------------------------------------------------------------------
 */
pid32	vcreate(
	  void		*funcaddr,	/* Address of the function	*/
	  uint32	ssize,		/* Stack size in words		*/
		uint32	hsize,		/* Heap size in num of pages */
	  pri16		priority,	/* Process priority > 0		*/
	  char		*name,		/* Name (for debugging)		*/
	  uint32	nargs,		/* Number of args that follow	*/
	  ...
	)
{
	uint32		savsp, *pushsp;
	intmask 	mask;    	/* Interrupt mask		*/
	pid32		pid;		/* Stores new process id	*/
	struct	procent	*prptr;		/* Pointer to proc. table entry */
	int32		i;
	uint32		*a;		/* Points to list of args	*/
	uint32		*saddr;		/* Stack address		*/

    kprintf(" Vcreating %s \n" , name ) ; 

	mask = disable();
	if (ssize < MINSTK)
		ssize = MINSTK;
	ssize = (uint32) roundew(ssize);
	if (((saddr = (uint32 *)getstk(ssize)) ==
	    (uint32 *)SYSERR ) ||
	    (pid=newpid()) == SYSERR || priority < 1 ) {
		restore(mask);
		return SYSERR;
	}

	prcount++;
	prptr = &proctab[pid];
  
  // Lab3 TODO. set up page directory, allocate bs etc.
#if EJ_LAB3
    kprintf(" Vcreating %s Lab 3 start \n" , name ) ; 
    // set PDBR
    uint32 free_frame_num = get_free_frame_number( pid , 0 , PAGE_DIR ) ; 
    prptr -> PDBR = Cal_Addr( free_frame_num ) ;
    pd_t* cur_pd = (pd_t*)( Cal_Addr( free_frame_num ) ) ;
    for( int i = 0 ; i < 1024 ; i ++){
        cur_pd -> pd_pres = 0 ; 
        cur_pd ++ ; 
        //clear_pd(cur_pd++) ; 
    }
    cur_pd = (pd_t*)( Cal_Addr( free_frame_num ) ) ;
    // set Global, Device page directories
    set_pd_t( cur_pd    , 1 + 1024 ) ;
    cur_pd ++ ; 
    set_pd_t( cur_pd    , 2 + 1024 ) ;
    cur_pd ++ ; 
    set_pd_t( cur_pd    , 3 + 1024 ) ;
    cur_pd ++ ; 
    set_pd_t( cur_pd    , 4 + 1024 ) ; 
    
    pd_t* device_pd_t = (pd_t*)Cal_Addr( free_frame_num ) ;
    for( int i = 0 ; i < 576 ; i ++ )
        device_pd_t++;
    set_pd_t( device_pd_t ,  5 + 1024 ) ;
    // create Virtual Heap 
    
	struct vmemblk *vmemptr;	/* Ptr to memory block		*/
    vmemptr = &( prptr->vmemlist ) ; 
    vmemptr->vaddr = (uint32)0x01000000 ; 
    vmemptr->vmlength = ( PAGE_SIZE * hsize ) ; 
    
    prptr->vminheap.vaddr = (uint32)0x01000000 ;
    prptr->vminheap.vnext = NULL ;
    prptr->vminheap.vmlength = (PAGE_SIZE * hsize ) ;
    
    prptr->vmaxheap.vaddr = (uint32)0x01000000 + (PAGE_SIZE * hsize ) ;
    prptr->vmaxheap.vnext = NULL ;
    prptr->vmaxheap.vmlength = 0 ;
    
    vmemptr->vnext = &prptr->vminheap;

    // Map BS 
    uint32 remain_hsize = hsize ;
    uint32 t = 0 ; 
    kprintf("Heap size = %d \n" , hsize ) ; 
    while( remain_hsize > MAX_PAGES_PER_BS ){
        kprintf("Remained size = %d \n" , remain_hsize ) ; 
        kprintf("Starting vpn  = %d \n" , t * MAX_PAGES_PER_BS ) ; 
        if( !bs_mapping( pid , MAX_PAGES_PER_BS , t * MAX_PAGES_PER_BS  )) 
            kprintf("bs_mapping error \n") ; 
        remain_hsize -= MAX_PAGES_PER_BS ; 
        t++ ; 
    }
    kprintf("Remained size = %d \n" , remain_hsize ) ; 
    kprintf("Starting vpn  = %d \n" , t * MAX_PAGES_PER_BS ) ; 
    if( !bs_mapping( pid , remain_hsize , t * MAX_PAGES_PER_BS )) 
        kprintf("bs_mapping error \n") ; 


    kprintf(" Vcreating %s Lab 3 end \n" , name ) ; 
#endif
  // END 

	/* Initialize process table entry for new process */
	prptr->prstate = PR_SUSP;	/* Initial state is suspended	*/
	prptr->prprio = priority;
	prptr->prstkbase = (char *)saddr;
	prptr->prstklen = ssize;
	prptr->prname[PNMLEN-1] = NULLCH;
	for (i=0 ; i<PNMLEN-1 && (prptr->prname[i]=name[i])!=NULLCH; i++)
		;
	prptr->prsem = -1;
	prptr->prparent = (pid32)getpid();
	prptr->prhasmsg = FALSE;

	/* Set up stdin, stdout, and stderr descriptors for the shell	*/
	prptr->prdesc[0] = CONSOLE;
	prptr->prdesc[1] = CONSOLE;
	prptr->prdesc[2] = CONSOLE;

	/* Initialize stack as if the process was called		*/

	*saddr = STACKMAGIC;
	savsp = (uint32)saddr;

	/* Push arguments */
	a = (uint32 *)(&nargs + 1);	/* Start of args		*/
	a += nargs -1;			/* Last argument		*/
	for ( ; nargs > 0 ; nargs--)	/* Machine dependent; copy args	*/
		*--saddr = *a--;	/*   onto created process' stack*/
	*--saddr = (long)INITRET;	/* Push on return address	*/

	/* The following entries on the stack must match what ctxsw	*/
	/*   expects a saved process state to contain: ret address,	*/
	/*   ebp, interrupt mask, flags, registerss, and an old SP	*/

	*--saddr = (long)funcaddr;	/* Make the stack look like it's*/
					/*   half-way through a call to	*/
					/*   ctxsw that "returns" to the*/
					/*   new process		*/
	*--saddr = savsp;		/* This will be register ebp	*/
					/*   for process exit		*/
	savsp = (uint32) saddr;		/* Start of frame for ctxsw	*/
	*--saddr = 0x00000200;		/* New process runs with	*/
					/*   interrupts enabled		*/

	/* Basically, the following emulates an x86 "pushal" instruction*/

	*--saddr = 0;			/* %eax */
	*--saddr = 0;			/* %ecx */
	*--saddr = 0;			/* %edx */
	*--saddr = 0;			/* %ebx */
	*--saddr = 0;			/* %esp; value filled in below	*/
	pushsp = saddr;			/* Remember this location	*/
	*--saddr = savsp;		/* %ebp (while finishing ctxsw)	*/
	*--saddr = 0;			/* %esi */
	*--saddr = 0;			/* %edi */
	*pushsp = (unsigned long) (prptr->prstkptr = (char *)saddr);
	
	restore(mask);
	kprintf("Create pid %d finished  \n" , pid );
	return pid;
}

/*------------------------------------------------------------------------
 *  newpid  -  Obtain a new (free) process ID
 *------------------------------------------------------------------------
 */
local	pid32	newpid(void)
{
	uint32	i;			/* Iterate through all processes*/
	static	pid32 nextpid = 1;	/* Position in table to try or	*/
					/*   one beyond end of table	*/

	/* Check all NPROC slots */

	for (i = 0; i < NPROC; i++) {
		nextpid %= NPROC;	/* Wrap around to beginning */
		if (proctab[nextpid].prstate == PR_FREE) {
			return nextpid++;
		} else {
			nextpid++;
		}
	}
	return (pid32) SYSERR;
}
