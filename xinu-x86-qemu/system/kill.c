/* kill.c - kill */

#include <xinu.h>

/*------------------------------------------------------------------------
 *  kill  -  Kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
syscall	kill(
	  pid32		pid		/* ID of process to kill	*/
	)
{
	intmask	mask;			/* Saved interrupt mask		*/
	struct	procent *prptr;		/* Ptr to process' table entry	*/
	int32	i;			/* Index into descriptors	*/

	mask = disable();
	if (isbadpid(pid) || (pid == NULLPROC)
	    || ((prptr = &proctab[pid])->prstate) == PR_FREE) {
		restore(mask);
		return SYSERR;
	}

	//kprintf("KILL : %d\n", pid);
	if (--prcount <= 1) {		/* Last user process completes	*/
		xdone();
	}
	
	send(prptr->prparent, pid);
	for (i=0; i<3; i++) {
		close(prptr->prdesc[i]);
	}

  // Lab3 TODO. Free frames as a process gets killed.
  // EJ
    for( i = 0 ; i < NFRAMES ; i++ ){
        if(ivptab[i].pid == pid){
            //reset_ivent()
            if( ivptab[i].valid == PAGE ){
                uint32 bstore , p_offset ; 
                get_store_offset( pid , ( ivptab[i].vpage_num << 12 ) , &bstore , &p_offset ) ; 
                kprintf("[write_bs] pid=[%d] ,vpn [%d] , page_num = [%d] , bstore = [%d] , p_offset = [%d] \n" , pid , ivptab[i].vpage_num  , i , bstore , p_offset) ; 
                if( write_bs( (char *)( ( i + 1024 ) * PAGE_SIZE ) , bstore , p_offset ) == SYSERR ) 
                    panic("wrtie_bs fails \n"); 
            }
            if( ivptab[i].valid == PAGE_TAB){
                hook_ptable_delete(i+1024) ; 
            } 
            
            ivptab[i].valid        = NULL_PAGE ; 
            ivptab[i].frame_number = i ; 
            ivptab[i].pid          = 0 ;  
            ivptab[i].vpage_num    = 0 ; 
            ivptab[i].ref_count    = 0 ; 
            if( ivptab[i].fifo_prev )
                ivptab[i].fifo_prev->fifo_next = ivptab[i].fifo_next ; 
            ivptab[i].fifo_next    = NULL ; 
            ivptab[i].fifo_prev    = NULL ; 
        } 
    }


    for( i = MIN_ID ; i <= MAX_ID ; i++ ){
        if(bsmap[i].pid == pid){
            deallocate_bs(i) ; 
            bsmap[i].ifMap       = 0 ;    
            bsmap[i].pid         = 0 ;    
            bsmap[i].num_pages   = 0 ;    
            bsmap[i].start_vpn   = 0 ;    
        }
    }

	freestk(prptr->prstkbase, prptr->prstklen);

	switch (prptr->prstate) {
	case PR_CURR:
		prptr->prstate = PR_FREE;	/* Suicide */
		resched();

	case PR_SLEEP:
	case PR_RECTIM:
		unsleep(pid);
		prptr->prstate = PR_FREE;
		break;

	case PR_WAIT:
		semtab[prptr->prsem].scount++;
		/* Fall through */

	case PR_READY:
		getitem(pid);		/* Remove from queue */
		/* Fall through */

	default:
		prptr->prstate = PR_FREE;
	}

	restore(mask);
	return OK;
}
