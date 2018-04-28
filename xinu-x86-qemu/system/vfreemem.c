/* vfreemem.c - vfreemem */

#include <xinu.h>

syscall	vfreemem(
	  char		*blkaddr,	/* Pointer to memory block	*/
	  uint32	nbytes		/* Size of block in bytes	*/
	)
{
  // Lab3 TODO.
	intmask	mask;			/* Saved interrupt mask		*/
	struct	vmemblk	*next, *prev , *new_vmemblk_ptr;
	uint32	top;
    
    struct procent *currPtr;	/* Ptr to table entry for cur process	*/
	currPtr = &proctab[currpid];

	mask = disable();
	if ((nbytes == 0) || ((uint32) blkaddr < (uint32) currPtr->vmemlist.vaddr )
			  || ((uint32) blkaddr >= (uint32) currPtr->vmaxheap.vaddr )) {
		restore(mask);
		return SYSERR;
	}

	nbytes = (uint32) roundmb(nbytes);	/* Use memblk multiples	*/
	//char * temp = getmem(sizeof(struct vmemblk)) ; 
    new_vmemblk_ptr = ( struct vmemblk* ) getmem(sizeof(struct vmemblk));
    new_vmemblk_ptr->vaddr = (uint32)blkaddr ; 
    //new_vmemblk_ptr->vmlength = nbytes ; 

	prev = &(currPtr->vmemlist);			/* Walk along free list	*/
	next = currPtr->vmemlist.vnext;
	while ((next != NULL) && (next->vaddr < (uint32)blkaddr)) {
		prev = next;
		next = next->vnext;
	}

	if (prev == &(currPtr->vmemlist)) {		/* Compute top of previous block*/
		top = NULL ;
	} else {
		top = prev->vaddr + prev->vmlength;
	}

	/* Ensure new block does not overlap previous or next blocks	*/

	if (((prev != &(currPtr->vmemlist)) && (uint32) blkaddr < top)
	    || ((next != NULL)	&& (uint32) blkaddr + nbytes > next->vaddr )  ) {
		restore(mask);
		return SYSERR;
	}

	currPtr -> vmemlist.vmlength += nbytes;

	/* Either coalesce with previous block or add to free list */

	if (top == (uint32) blkaddr ) { 	/* Coalesce with previous block	*/
		prev->vmlength += nbytes;
        freemem( (char *)new_vmemblk_ptr , sizeof(struct vmemblk)) ; 
        new_vmemblk_ptr = prev;
	} else {			/* Link into list as new node	*/
		new_vmemblk_ptr->vnext = next;
		new_vmemblk_ptr->vmlength = nbytes;
		prev->vnext = new_vmemblk_ptr;
	}

	/* Coalesce with next block if adjacent */

	if ((new_vmemblk_ptr->vaddr + new_vmemblk_ptr->vmlength) == next->vaddr) {
		new_vmemblk_ptr->vmlength += next->vmlength;
		new_vmemblk_ptr->vnext    = next->vnext;
	}
	restore(mask);
	return OK;
}
