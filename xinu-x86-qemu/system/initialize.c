/* initialize.c - nulluser, sysinit, sizmem */

/* Handle system initialization and become the null process */

#include <xinu.h>
#include <string.h>
#include <lab3.h>

extern	void	start(void);	/* Start of Xinu code			*/
extern	void	*_end;		/* End of Xinu code			*/

/* Function prototypes */

extern	void main(void);	/* Main is the first process created	*/
extern	void xdone(void);	/* System "shutdown" procedure		*/
static	void sysinit(); 	/* Internal system initialization	*/
extern	void meminit(void);	/* Initializes the free memory list	*/

/* Lab3. initializes data structures and necessary set ups for paging */
static	void initialize_paging();

//unsigned long Cal_Addr( unsigned long frame_number){
uint32 Cal_Addr( uint64 frame_number){
    return ( frame_number + 1024 ) * PAGE_SIZE ; 
}

void set_pd_t(pd_t* pd , unsigned int page_number){
        pd_t temp ; 
        temp.pd_pres   = 1  ;       //     /* page table present?      
        temp.pd_write  = 1  ;       //     /* page is writable?        
        temp.pd_user   = 0  ;       //     /* is use level protection? 
        temp.pd_pwt    = 0  ;       //     /* write through cachine for pt? 
        temp.pd_pcd    = 0  ;       //     /* cache disable for this pt?   
        temp.pd_acc    = 0  ;       //     /* page table was accessed? 
        temp.pd_mbz    = 0  ;       // /* must be zero         
        temp.pd_fmb    = 0  ;       // /* four MB pages?       
        temp.pd_global = 0  ;               // /* global (ignored)     
        temp.pd_avail  = 0  ;               // /* for programmer's use     
        temp.pd_base   = page_number  ; //     /* location of page table?  
        *pd = temp ; 
}
void set_pt_t(pt_t* pt , unsigned int page_number){
        pt_t temp ; 
        temp.pt_pres    = 1  ;       //     /* page table present?      
        temp.pt_write   = 1  ;       //     /* page is writable?        
        temp.pt_user    = 0  ;       //     /* is use level protection? 
        temp.pt_pwt     = 0  ;       //     /* write through cachine for pt? 
        temp.pt_pcd     = 0  ;       //     /* cache disable for this pt?   
        temp.pt_acc     = 0  ;       //     /* page table was accessed? 
        temp.pt_dirty   = 0  ;       // /* must be zero         
        temp.pt_mbz     = 0  ;       // /* four MB pages?       
        temp.pt_global  = 0  ;               // /* global (ignored)     
        temp.pt_avail   = 0  ;               // /* for programmer's use     
        temp.pt_base    = page_number  ; //     /* location of page table?  
        *pt = temp ; 
}

//unsigned long tmp ;
uint64 tmp ;
//void write_CR( unsigned long cr , int idx){
void write_CR( uint64 cr , int idx){
    
	intmask		mask;		/* Saved interrupt mask		*/
	mask = disable();

    tmp = cr ; 
    asm("pushl %eax");
    asm("movl tmp, %eax");		/* mov (move) value at tmp into %eax register.
					            "l" signifies long (see docs on gas assembler)	*/
    if( idx == 0)
        asm("movl %eax, %cr0");
    else if ( idx == 3)
        asm("movl %eax, %cr3");
    else if ( idx == 4)
        asm("movl %eax, %cr4");
    else{
        kprintf("You should not touch this reg \n") ; 
        restore(mask);
        return ; 
    }
    asm("popl %eax");

  restore(mask);
  return ;
} 

//unsigned long read_CR(unsigned long cr , int idx) {
uint64 read_CR(uint64 cr , int idx) {
  
    intmask		mask;		/* Saved interrupt mask		*/
    mask = disable();
    //unsigned long local_tmp;
    uint64 local_tmp;


    asm("pushl %eax");
    if( idx == 0)
        asm("movl %cr0, %eax");
    else if ( idx == 2)
        asm("movl %cr2, %eax");
    else if ( idx == 3)
        asm("movl %cr3, %eax");
    else if ( idx == 4)
        asm("movl %cr4, %eax");
    else{
        kprintf("You should not touch this reg \n") ; 
        restore(mask);
        return -1 ;
    }
    asm("movl %eax, tmp");
    asm("popl %eax");

    local_tmp = tmp;
    //cr = local_tmp ; 

    restore(mask);
    return local_tmp ;
}

void dump32(uint64 n) {
    intmask		mask;		/* Saved interrupt mask		*/
    mask = disable();
    int32 i;

    for(i = 31; i>=0; i--) {
        kprintf("%02d ",i);
    }

    kprintf("\n");

    for(i=31;i>=0;i--)
        kprintf("%d  ", (n&(1<<i)) >> i);

    kprintf("\n");
    restore(mask);
}




/* Declarations of major kernel variables */

struct	procent	proctab[NPROC];	/* Process table			*/
struct	sentry	semtab[NSEM];	/* Semaphore table			*/
struct	memblk	memlist;	/* List of free memory blocks		*/

/* Lab3. frames metadata handling */
frame_md_t frame_md;
struct ivent ivptab[NFRAMES] ; 
struct bsent bsmap[8] ; 
uint64 check_CR3 ; 
uint32 num_fault ; 
sid32  get_frame_sem ; 
sid32  pgf_frame_sem ; 

struct ivent*   fifo_head  ;
struct ivent*   fifo_hand  ;
struct ivent*   fifo_tail  ;
struct ivent    fifo_head_dummy_entry  ;
struct ivent    fifo_tail_dummy_entry  ;

/* Active system status */

int	prcount;		/* Total number of live processes	*/
pid32	currpid;		/* ID of currently executing process	*/

bool8   PAGE_SERVER_STATUS;    /* Indicate the status of the page server */
sid32   bs_init_sem;

/*------------------------------------------------------------------------
 * nulluser - initialize the system and become the null process
 *
 * Note: execution begins here after the C run-time environment has been
 * established.  Interrupts are initially DISABLED, and must eventually
 * be enabled explicitly.  The code turns itself into the null process
 * after initialization.  Because it must always remain ready to execute,
 * the null process cannot execute code that might cause it to be
 * suspended, wait for a semaphore, put to sleep, or exit.  In
 * particular, the code must not perform I/O except for polled versions
 * such as kprintf.
 *------------------------------------------------------------------------
 */

void	nulluser()
{
	struct	memblk	*memptr;	/* Ptr to memory block		*/
	uint32	free_mem;		/* Total amount of free memory	*/

	/* Initialize the system */

	sysinit();

  // Lab3
	initialize_paging();

	kprintf("\n\r%s\n\n\r", VERSION);

	/* Output Xinu memory layout */
	free_mem = 0;
	for (memptr = memlist.mnext; memptr != NULL;
						memptr = memptr->mnext) {
		free_mem += memptr->mlength;
	}
	kprintf("%10d bytes of free memory.  Free list:\n", free_mem);
	for (memptr=memlist.mnext; memptr!=NULL;memptr = memptr->mnext) {
	    kprintf("           [0x%08X to 0x%08X]\r\n",
		(uint32)memptr, ((uint32)memptr) + memptr->mlength - 1);
	}

	kprintf("%10d bytes of Xinu code.\n",
		(uint32)&etext - (uint32)&text);
	kprintf("           [0x%08X to 0x%08X]\n",
		(uint32)&text, (uint32)&etext - 1);
	kprintf("%10d bytes of data.\n",
		(uint32)&ebss - (uint32)&data);
	kprintf("           [0x%08X to 0x%08X]\n\n",
		(uint32)&data, (uint32)&ebss - 1);

	/* Create the RDS process */

	//rdstab[0].rd_comproc = create(rdsprocess, RD_STACK, RD_PRIO,
	//				"rdsproc", 1, &rdstab[0]);
	rdstab[0].rd_comproc = vcreate(rdsprocess, RD_STACK, 0 , RD_PRIO,
					"rdsproc", 1, &rdstab[0]);
	if(rdstab[0].rd_comproc == SYSERR) {
		panic("Cannot create remote disk process");
	}
	resume(rdstab[0].rd_comproc);

	/* Enable interrupts */

	enable();

	/* Create a process to execute function main() */

    pid32 main_pid = vcreate((void *)main, INITSTK, 0 , INITPRIO, "Main process", 0, NULL) ; 
	if( main_pid == SYSERR) {
		kprintf( "Cannot create main process\n" );
	}
	resume ( main_pid ) ; 
	   //vcreate((void *)main, INITSTK, 0 , INITPRIO, "Main process", 0,
       //    NULL));
	   //create((void *)main, INITSTK, INITPRIO, "Main process", 0,
       //    NULL));

	/* Become the Null process (i.e., guarantee that the CPU has	*/
	/*  something to run when no other process is ready to execute)	*/
    
	while (TRUE) {
		;		/* Do nothing */
	}

}

/*------------------------------------------------------------------------
 *
 * sysinit  -  Initialize all Xinu data structures and devices
 *
 *------------------------------------------------------------------------
 */
static	void	sysinit()
{
	int32	i;
	struct	procent	*prptr;		/* Ptr to process table entry	*/
	struct	sentry	*semptr;	/* Ptr to semaphore table entry	*/

	/* Platform Specific Initialization */

	platinit();

	/* Initialize the interrupt vectors */

	initevec();

	/* Initialize free memory list */

	meminit();

	/* Initialize system variables */

	/* Count the Null process as the first process in the system */

	prcount = 1;

	/* Scheduling is not currently blocked */

	Defer.ndefers = 0;

	/* Initialize process table entries free */

	for (i = 0; i < NPROC; i++) {
		prptr = &proctab[i];
		prptr->prstate = PR_FREE;
		prptr->prname[0] = NULLCH;
		prptr->prstkbase = NULL;
		prptr->prprio = 0;
	}

	/* Initialize the Null process entry */

	prptr = &proctab[NULLPROC];
	prptr->prstate = PR_CURR;
	prptr->prprio = 0;
	strncpy(prptr->prname, "prnull", 7);
	prptr->prstkbase = getstk(NULLSTK);
	prptr->prstklen = NULLSTK;
	prptr->prstkptr = 0;
	currpid = NULLPROC;

	/* Initialize semaphores */

	for (i = 0; i < NSEM; i++) {
		semptr = &semtab[i];
		semptr->sstate = S_FREE;
		semptr->scount = 0;
		semptr->squeue = newqueue();
	}

	/* Initialize buffer pools */

	bufinit();

	/* Create a ready list for processes */

	readylist = newqueue();

	/* Initialize the real time clock */

	clkinit();

#ifndef QEMU
	for (i = 0; i < NDEVS; i++) {
		init(i);
	}
#else
    init(QEMUCONSOLE);
    init(QEMURDISK);
#endif

	PAGE_SERVER_STATUS = PAGE_SERVER_INACTIVE;
	bs_init_sem = semcreate(1);

	return;
}

static void initialize_paging()
{
#if EJ_LAB3
    /* LAB3 TODO */
    num_fault = 0  ; 
    fifo_head = &fifo_head_dummy_entry ; 
    fifo_hand = fifo_head ; 
    fifo_tail = &fifo_tail_dummy_entry ; 
    
    fifo_head_dummy_entry.fifo_next = NULL ; 
    fifo_head_dummy_entry.fifo_prev = fifo_tail ; 
    fifo_head_dummy_entry.frame_number = (uint32) -1 ; 
    fifo_tail_dummy_entry.fifo_next = fifo_head ; 
    fifo_tail_dummy_entry.fifo_prev = NULL ; 
    fifo_tail_dummy_entry.frame_number = (uint32) -1 ; 
    
    get_frame_sem = semcreate(1) ; 
    pgf_frame_sem = semcreate(1) ; 
    //1. set_IVTab() ; 
    #if EJ_debug
    kprintf("reste_ivtentry \n") ; 
    #endif
    reset_ivtentry() ;
    // null_pd , global_pt , device_pt 
    #if EJ_debug
    kprintf("set ivtentry pt,pd \n") ; 
    #endif
    set_ivtentry_pd_pt( 0 , 0 , PAGE_DIR ) ;
    for( uint32 i = 1 ; i < 6 ; i++){
        set_ivtentry_pd_pt( i , 0 , PAGE_TAB ) ;
    }
    //2. set_BSmapTab() ; 
    #if EJ_debug
    kprintf("set BS map\n") ; 
    #endif
    reset_BSmapTab() ; 

    //3. set Null process's paging members.
    #if EJ_debug
    kprintf("set Null process's page members\n") ; 
    #endif
	struct	procent	*nullptr;		/* Ptr to process table entry	*/
	nullptr = &proctab[NULLPROC];
    nullptr -> PDBR = Cal_Addr( 0 ) ; 
    //4. assign Null process's page directories to global page table
    pd_t* null_pd = (pd_t*)Cal_Addr( 0 ) ;
    for( int i = 0 ; i < 1024 ; i ++){
        null_pd -> pd_pres = 0 ; 
        null_pd ++ ; 
    }
    null_pd = (pd_t*)Cal_Addr( 0 ) ;
    set_pd_t( null_pd , 1 + 1024 ) ;
    null_pd ++ ; 
    set_pd_t( null_pd , 2 + 1024 ) ;
    null_pd ++ ; 
    set_pd_t( null_pd , 3 + 1024 ) ;
    null_pd ++ ; 
    set_pd_t( null_pd , 4 + 1024 ) ; 
    // 5. set gloabl page tables
    #if EJ_debug
    kprintf("set global page tables\n") ; 
    #endif
    hook_ptable_create(1+1024); 
    hook_ptable_create(2+1024); 
    hook_ptable_create(3+1024); 
    hook_ptable_create(4+1024); 
    pt_t* g_pt_t = (pt_t*)Cal_Addr( 1 ) ; 
    for( int p = 0 ; p < 4 ; p ++){
        for( int i = 0 ; i < 1024 ; i ++ ){
            set_pt_t( g_pt_t , p * 1024 + i ) ;
            g_pt_t++ ; 
        }
    }
    // 6. set device memory pd and pt
    #if EJ_debug
    kprintf("set device page tables\n") ; 
    #endif
    pd_t* device_pd_t = (pd_t*)Cal_Addr( 0 ) ;
    for( int i = 0 ; i < 576 ; i ++ )
        device_pd_t++;
    set_pd_t( device_pd_t ,  5 + 1024 ) ;
    
    hook_ptable_create(5+1024); 
    pt_t* device_pt_t = (pt_t*)Cal_Addr( 5 ) ; 
    for( int i = 0 ; i < 1024 ; i ++)
        set_pt_t( device_pt_t++ , 576*1024 + i ) ; 
    
    // 7. set CR3 / PDBR 
    #if EJ_debug
    kprintf("set CR3\n") ; 
    #endif
    write_CR( nullptr -> PDBR , 3 ) ;
    //check_CR3 = nullptr -> PDBR ;  
    uint64 ch_cr3 = 1 ; 
    ch_cr3 = read_CR( ch_cr3 , 3 ) ;
    dump32( ch_cr3 ) ; 
    dump32( nullptr->PDBR ) ; 
    // 8. install ISR
    #if EJ_debug
    kprintf("set ISR\n") ; 
    #endif
    set_evec( 14 , (uint32) pagefault ) ; 

    // 9.enable page table 
    #if EJ_debug
    kprintf("set CR4\n") ; 
    #endif
    uint64 cr4 = 0 ;
    cr4 = read_CR( cr4 , 4  ) ;
    cr4 = cr4 & (~(1 << 5 ) )  ;
    write_CR( cr4 , 4) ;
    dump32( cr4 ) ; 
    
    
    kprintf("set CR0\n") ; 
    uint64 cr0 = 1 ; 
    cr0 = read_CR( cr0 , 0 ) ;
    dump32( cr0 ) ; 
    #if EJ_debug
    kprintf("cr0 = \n" , (uint32)cr0 ) ; 
    #endif
    cr0 = cr0 | 0x80000001 ; //(1 << 31 ) | 1 ;
    dump32( cr0 ) ; 
    write_CR( cr0 , 0) ; 
    #if EJ_debug
    dump32( cr0 ) ; 
    cr0 = read_CR( cr0 , 0 ) ;
    kprintf("cr0 = \n" , (uint32)cr0 ) ; 
    #endif
#endif
    //
    kprintf("Finish init page\n") ; 
    #if EJ_debug
    kprintf("Finish init page\n") ; 
    #endif
	return;
}

int32	stop(char *s)
{
	kprintf("%s\n", s);
	kprintf("looping... press reset\n");
	while(1)
		/* Empty */;
}

int32	delay(int n)
{
	DELAY(n);
	return OK;
}


//EJ Util functions
uint32 get_free_frame_number( uint32 pid , uint32 vpn , uint32 purpose){
    wait(get_frame_sem) ; 
    intmask	mask;			/* Saved interrupt mask		*/
    mask = disable();
    
    /*struct ivent* temp = fifo_head ; 
    kprintf("Dump fifo\n") ; 
    while( temp != NULL){
        kprintf("%d -> " , temp->frame_number) ; 
        temp = temp->fifo_prev ; 
    } */

    for( uint32 i = 0 ; i < NFRAMES ; i++){
        if( ivptab[i].valid == NULL_PAGE ){
            ivptab[i].valid = purpose ; 
            ivptab[i].pid = pid ;
            if( purpose == PAGE ){
                ivptab[i].vpage_num = vpn ; 
               
                ivptab[i].fifo_prev = fifo_tail ; 
                ivptab[i].fifo_next = fifo_tail->fifo_next; 
                ivptab[i].fifo_next->fifo_prev = &ivptab[i]; 
                fifo_tail->fifo_next = &ivptab[i] ; 
            }    
    //        kprintf(" Get free frame number [%d] with no replacement " , i ) ; 
            restore(mask);
            kprintf(" Get free frame number for pid %d , vpn %d , purpose %d , ret %d \n" , pid , vpn , purpose , i  ) ; 
            signal(get_frame_sem) ; 
            return i ; 
        }
    }
    
    
    //kprintf(" Get free frame number [%d] with replacement " , ret ) ; 
    if( fifo_head->fifo_prev == NULL){
        panic("fifo error\n") ; 
    }
    //TODO if FIFO
    uint32 ret = (uint32) -1  ; 
    if(currpolicy == FIFO){ 
        ret      = fifo_head->fifo_prev->frame_number      ; 
        if( ivptab[ret].valid != PAGE){
            panic("replaced frame should be PAGE\n") ; 
        }
        ivptab[ret].fifo_prev->fifo_next = ivptab[ret].fifo_next ; 
        ivptab[ret].fifo_next->fifo_prev = ivptab[ret].fifo_prev ; 
        ivptab[ret].fifo_next    = NULL ; 
        ivptab[ret].fifo_prev    = NULL ; 
        
        //fifo_head->fifo_prev = fifo_head->fifo_prev->fifo_prev    ; 
        //fifo_head->fifo_prev->fifo_next = fifo_head    ; 
    }else if( currpolicy == GCA ){  // else if GCA
        
        while( 1 ){
            //check validation
            if( fifo_hand == fifo_head ){  // head means it is the first access ; 
                fifo_hand = fifo_head -> fifo_prev ; 
            }else if( fifo_hand == fifo_tail){ // tail means error ; 
                panic("error, should never happen \n ") ; 
            }
            
            uint32 gca_pid = fifo_hand->pid ;
            uint32 gca_vpn = fifo_hand->vpage_num ;
            uint32 gca_vaddr = gca_vpn * PAGE_SIZE ; 
            uint32 gca_p = gca_vaddr >> 22 ; 
            uint32 gca_q = gca_vaddr >> 12 & 0x000003ff ;  
   
            pd_t* gca_pd = (pd_t*)(uint32)( proctab[gca_pid].PDBR ) ;
            for ( int i = 0 ; i < gca_p ; i++){
                gca_pd ++ ; 
            }
            pt_t* gca_pt = (pt_t*)Cal_Addr( gca_pd -> pd_base - 1024 ) ; 
            for ( int i = 0 ; i < gca_q ; i++){
                gca_pt ++ ; 
            }
            
            if(gca_pt->pt_acc == 0 && gca_pt->pt_dirty == 0  ){         // if (0,0)
                ret = fifo_hand -> frame_number ; 
                fifo_hand = fifo_hand -> fifo_prev ;  //  hand ++ ; 
                break ; 
            }else if( gca_pt->pt_acc == 1 && gca_pt->pt_dirty == 0  ){  // if (1,0)
                gca_pt->pt_acc = 0  ;                // set (0,0)
                fifo_hand = fifo_hand -> fifo_prev ;  //  hand ++ ; 
            }else if( gca_pt->pt_acc == 1 && gca_pt->pt_dirty == 1  ){  // if (1,1)
                gca_pt->pt_dirty = 0  ;              // set (1,0)
                fifo_hand = fifo_hand -> fifo_prev ;  //  hand ++ ; 
            }else{
                panic("error \n" ) ; 
            }
            if( fifo_hand == fifo_tail ){
                fifo_hand = fifo_head -> fifo_prev ; 
            }
        }
        
        ivptab[ret].fifo_prev->fifo_next = ivptab[ret].fifo_next ; 
        ivptab[ret].fifo_next->fifo_prev = ivptab[ret].fifo_prev ; 
        ivptab[ret].fifo_next    = NULL ; 
        ivptab[ret].fifo_prev    = NULL ; 
        
    }else{
        panic("policy error \n") ; 
    }
    
    //kprintf(" Get free frame number [%d] with replacement " , ret ) ; 
   
    uint32 r_pid = ivptab[ret].pid ; 
    uint32 r_vpn = ivptab[ret].vpage_num ;

    uint32 r_vaddr = r_vpn * PAGE_SIZE ; 
    uint32 r_p = r_vaddr >> 22 ; 
    uint32 r_q = r_vaddr >> 12 & 0x000003ff ;  
   

    //pd_t* r_pd = (pd_t*)(uint32)( proctab[r_pid].PDBR + 4 * r_p ) ;
    pd_t* r_pd = (pd_t*)(uint32)( proctab[r_pid].PDBR ) ;
    for ( int i = 0 ; i < r_p ; i++){
        r_pd ++ ; 
    }
    //pt_t* r_pt = (pt_t*)( r_pd -> pd_base + 4 * r_q ) ; 
    pt_t* r_pt = (pt_t*)Cal_Addr( r_pd -> pd_base - 1024 ) ; 
    for ( int i = 0 ; i < r_q ; i++){
        r_pt ++ ; 
    }

    //if( r_pt->pt_dirty == 1 ){
    if( 1 ){
        uint32 bstore , p_offset ; 
        get_store_offset( r_pid , r_vaddr , &bstore , &p_offset ) ; 
        kprintf("[write_bs] pid = [%d] , vpn = [%d], page_num = [%d] , bstore = [%d] , p_offset = [%d] \n" , r_pid , r_vpn , ret , bstore , p_offset) ; 
        if( write_bs( (char *)( ( ret + 1024 ) * PAGE_SIZE ) , bstore , p_offset ) == SYSERR )
            panic("write_bs_fail \n") ; 
    }
    
    r_pt -> pt_pres = 0 ;
    if( currpid == r_pid ) 
        asm volatile("invlpg (%0)" ::"r" ((uint64)r_vaddr) : "memory");
    
    if( ivptab[ r_pd-> pd_base - 1024].valid != PAGE_TAB ){
        panic("ref_count error  \n");
    } 
    ivptab[ r_pd->pd_base - 1024].ref_count -- ; 
    if ( ivptab[ r_pd->pd_base - 1024].ref_count == 0 ){
        hook_ptable_delete( r_pd->pd_base ) ; 
        r_pd -> pd_pres = 0 ; 
        ivptab[r_pd->pd_base - 1024].valid = NULL_PAGE ; 
    }
    
    pid32 old_pid     = ivptab[ret].pid ; 
    pid32 old_pagenum = ivptab[ret].vpage_num ; 
    ivptab[ret].valid = purpose ; 
    ivptab[ret].pid = pid ;
    if( purpose == PAGE ){
        ivptab[ret].vpage_num = vpn ; 
    
        ivptab[ret].fifo_prev = fifo_tail ; 
        ivptab[ret].fifo_next = fifo_tail->fifo_next; 
        ivptab[ret].fifo_next->fifo_prev = &ivptab[ret]; 
        fifo_tail->fifo_next = &ivptab[ret] ; 
    } 
    hook_pswap_out( old_pid , old_pagenum , ret  );
    restore(mask);
    kprintf(" Get free frame number for pid %d , vpn %d , purpose %d , ret %d \n" , pid , vpn , purpose , ret  ) ; 
    signal(get_frame_sem) ; 
    return ret ; 
}

void myPageFault(){

    wait(pgf_frame_sem);	
    intmask	mask;			/* Saved interrupt mask		*/
    uint64 pf_addr = 0 ;
    uint32 v_pn , v_pd_idx , v_pt_idx ;
    //uint32 store, p_offset , free_frame_num ;
    struct procent* currPtr ; 
    pd_t* curr_pd    ; 
    pt_t* curr_pt    ; 

    
    mask = disable();
   
    kprintf("start Page fault handler\n") ; 
    
    num_fault ++ ; 

    currPtr = &proctab[currpid] ; 
    pf_addr = read_CR( pf_addr , 2 ) ; // read faulted address ; 
    //check if a is valid 
    if( pf_addr < currPtr->vmemlist.vaddr || pf_addr >= currPtr->vmaxheap.vaddr ){
        kprintf("faulted address is not valid Ox%x , pid = %d \n" , (uint32)pf_addr , currpid ) ; 
        dump32(pf_addr); 
        //kill(currpid) ;
        restore(mask);
        signal(pgf_frame_sem);
        return ; 
        // TODO kill the process ??? 
    }
    
    v_pn = pf_addr >> 12 ; 
    v_pd_idx = v_pn >> 10 ;
    v_pt_idx = v_pn & (0x000003FF) ; 
    
    //curr_pd = (pd_t*)( (uint32)(currPtr -> PDBR) + 4 * v_pd_idx ) ; 
    
    curr_pd = (pd_t*)(uint32)( currPtr -> PDBR ) ;
    for ( int i = 0 ; i < v_pd_idx ; i++){
        curr_pd ++ ;
    } 
    
    if( curr_pd -> pd_pres == 0 ){ // the page table does not exist 
        create_pagetable( curr_pd ) ;  
        //curr_pt = (pt_t*)( Cal_Addr( curr_pd->pd_base - 1024) + 4 * v_pt_idx  ) ; 
        curr_pt = (pt_t*)( Cal_Addr( curr_pd->pd_base - 1024) ) ; 
        for ( int i = 0 ; i < v_pt_idx ; i++){
            curr_pt ++ ;
        } 
    }else{
        //curr_pt = (pt_t*)( Cal_Addr( curr_pd->pd_base - 1024) + 4 * v_pt_idx  ) ; 
        curr_pt = (pt_t*)( Cal_Addr( curr_pd->pd_base - 1024) ) ; 
        for ( int i = 0 ; i < v_pt_idx ; i++){
            curr_pt ++ ;
        }    
    }
    
    uint32 bstore , p_offset ; 
    get_store_offset( currpid , pf_addr , &bstore , &p_offset ) ; 
    uint32 free_frame_num = get_free_frame_number( currpid , v_pn , PAGE ) ; 
    
    kprintf("[read_bs] pid = [%d] , vpn = [%d], page_num = [%d] , bstore = [%d] , p_offset = [%d] \n" , currpid , v_pn , free_frame_num , bstore , p_offset) ; 
    if( read_bs( (char*)( ( free_frame_num + 1024) * PAGE_SIZE ) , bstore , p_offset ) == SYSERR) 
        panic("read_bs fail \n"); 
     
    
    set_pt_t( curr_pt , free_frame_num + 1024 ) ; 
    ivptab[ curr_pd->pd_base-1024 ].ref_count ++ ; 

    hook_pfault( currpid , (void*)(uint32)pf_addr , v_pn , free_frame_num ) ; 
    restore(mask);
    //kprintf("entering page fault \n") ; 
    signal(pgf_frame_sem);
    return ;  
}

void create_pagetable(pd_t* pd){
    uint32 free_frame_num = get_free_frame_number( currpid , 0 , PAGE_TAB ) ;
    hook_ptable_create(free_frame_num+1024) ;
    set_pd_t( pd , free_frame_num + 1024 ) ; 
    pt_t* pt = (pt_t*)Cal_Addr( free_frame_num ) ; 
    for( int i = 0 ; i < 1024 ; i ++){
        set_pt_t( pt , 0 ) ;
        pt -> pt_pres = 0 ;
        pt ++ ; 
    }
}

void reset_ivtentry(){
    for( int i = 0 ; i < NFRAMES ; i++ ){
        ivptab[i].valid        = NULL_PAGE ; 
        ivptab[i].frame_number = i ; 
        ivptab[i].pid          = 0 ;  
        ivptab[i].vpage_num    = 0 ; 
        ivptab[i].ref_count    = 0 ; 
        ivptab[i].fifo_next    = NULL ; 
        ivptab[i].fifo_prev    = NULL ; 
    }
}
void set_ivtentry_pd_pt( uint32 i , uint32 pid, uint32 type){
    if ( type != PAGE_DIR && type!= PAGE_TAB){ kprintf("!!error!! \n") ; }
    ivptab[i].valid        = type ; 
    ivptab[i].frame_number = i ; 
    ivptab[i].pid          = pid ;  
}

bool8 bs_mapping( uint32 pid , uint32 n_pages , uint32 vpn  ){
    if( n_pages == 0 )
        return 1 ; 
    bsd_t store = allocate_bs(n_pages) ; 
    kprintf("BS_allocate store = %d \n" , store ) ; 

    if( bsmap[store].ifMap == 0 ){
        bsmap[store].ifMap       = 1 ;
        bsmap[store].pid         = pid ;    
        bsmap[store].num_pages   = n_pages ;    
        bsmap[store].start_vpn   = vpn + 4096 ;    
        return 1 ; 
    }else{
        kprintf("bs_mapping_allocate error \n") ; 
    }


    /*
    for( uint32 i = MIN_ID ; i <= MAX_ID ; i++ ){
        if( bsmap[i].ifMap == 0 ){
            bsmap[i].ifMap       = 1 ;
            bsmap[i].pid         = pid ;    
            bsmap[i].num_pages   = n_pages ;    
            bsmap[i].start_vpn   = vpn ;    
            return 1 ; 
        }
    }*/
    return 0 ; 
}

void reset_BSmapTab(){
    for( uint32 i = MIN_ID ; i <= MAX_ID ; i++ ){
        bsmap[i].ifMap       = 0 ;    
        bsmap[i].pid         = 0 ;    
        bsmap[i].num_pages   = 0 ;    
        bsmap[i].start_vpn   = 0 ;    
    }
}
void get_store_offset( uint32 pid, uint32 vaddr, uint32* store , uint32* offset ){
    uint32 vpn = vaddr >> 12 ; 
    for( uint32 i = MIN_ID ; i <= MAX_ID ; i++ ){
        if( pid == bsmap[i].pid ){
            if( vpn >= bsmap[i].start_vpn && vpn - bsmap[i].start_vpn < bsmap[i].num_pages ){
                *store = i ; 
                *offset = vpn - bsmap[i].start_vpn ; 
                return ; 
            }
        }
    }
    //kprintf("BSmap Finding error \n" ) ; 
}

void clear_pd(pd_t* pd){
       pd->pd_pres   = 0  ;       //     /* page table present?      
       pd->pd_write  = 0  ;       //     /* page is writable?        
       pd->pd_user   = 0  ;       //     /* is use level protection? 
       pd->pd_pwt    = 0  ;       //     /* write through cachine for pt? 
       pd->pd_pcd    = 0  ;       //     /* cache disable for this pt?   
       pd->pd_acc    = 0  ;       //     /* page table was accessed? 
       pd->pd_mbz    = 0  ;       // /* must be zero         
       pd->pd_fmb    = 0  ;       // /* four MB pages?       
       pd->pd_global = 0  ;               // /* global (ignored)     
       pd->pd_avail  = 0  ;               // /* for programmer's use     
       pd->pd_base   = 0  ; 
}
void clear_pt(pt_t* pt){
        pt->pt_pres    = 0  ;       //     /* page table present?      
        pt->pt_write   = 0  ;       //     /* page is writable?        
        pt->pt_user    = 0  ;       //     /* is use level protection? 
        pt->pt_pwt     = 0  ;       //     /* write through cachine for pt? 
        pt->pt_pcd     = 0  ;       //     /* cache disable for this pt?   
        pt->pt_acc     = 0  ;       //     /* page table was accessed? 
        pt->pt_dirty   = 0  ;       // /* must be zero         
        pt->pt_mbz     = 0  ;       // /* four MB pages?       
        pt->pt_global  = 0  ;               // /* global (ignored)     
        pt->pt_avail   = 0  ;               // /* for programmer's use     
        pt->pt_base    = 0  ; 
}

