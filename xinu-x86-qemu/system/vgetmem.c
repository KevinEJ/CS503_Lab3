/* vgetmem.c - vgetmem */

#include <xinu.h>

char  	*vgetmem(
	  uint32	nbytes		/* Size of memory requested	*/
	)
{
  // Lab3 TODO.
	intmask	mask;			/* Saved interrupt mask		*/
	struct	vmemblk	*prev, *curr ;
	
    struct procent *currPtr;	/* Ptr to table entry for cur process	*/
	currPtr = &proctab[currpid];

    mask = disable();
	if (nbytes == 0) {
		restore(mask);
		return (char *)SYSERR;
	}
	
	nbytes = (uint32) roundmb(nbytes);	/* Use memblk multiples	*/
    
    prev = &(currPtr->vmemlist);
	curr = currPtr->vmemlist.vnext;
	while (curr != NULL) {			/* Search free list	*/

		if (curr->vmlength == nbytes) {	/* Block is exact match	*/
			prev->vnext = curr->vnext;
			currPtr -> vmemlist.vmlength -= nbytes;
			restore(mask);
			return (char *)(curr->vaddr);

		} else if (curr->vmlength > nbytes) { /* Split big block	*/
			char* output = (char*) curr->vaddr ; 
            curr->vaddr += nbytes ; 
            curr->vmlength -= nbytes ; 
            currPtr -> vmemlist.vmlength -= nbytes;
			restore(mask);
			return output;
		} else {			/* Move to next block	*/
			prev = curr;
			curr = curr->vnext;
		}
	}
	restore(mask);
    return (char *)SYSERR;
}
