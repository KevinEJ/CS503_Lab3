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

unsigned long Cal_Addr( unsigned long frame_number){
    return ( frame_number + 1024 ) * 4096 ; 
}

void set_pd_t(pd_t* pd , unsigned int page_number){
        pd -> pd_pres   = 1  ;       //     /* page table present?      
        pd -> pd_write  = 1  ;       //     /* page is writable?        
        pd -> pd_user   = 0  ;       //     /* is use level protection? 
        pd -> pd_pwt    = 0  ;       //     /* write through cachine for pt? 
        pd -> pd_pcd    = 0  ;       //     /* cache disable for this pt?   
        pd -> pd_acc    = 0  ;       //     /* page table was accessed? 
        pd -> pd_mbz    = 0  ;       // /* must be zero         
        pd -> pd_fmb    = 0  ;       // /* four MB pages?       
        pd -> pd_global = 0  ;               // /* global (ignored)     
        pd -> pd_avail  = 0  ;               // /* for programmer's use     
        pd -> pd_base   = page_number  ; //     /* location of page table?  
}
void set_pt_t(pt_t* pt , unsigned int page_number){
        pt -> pt_pres    = 1  ;       //     /* page table present?      
        pt -> pt_write   = 1  ;       //     /* page is writable?        
        pt -> pt_user    = 0  ;       //     /* is use level protection? 
        pt -> pt_pwt     = 0  ;       //     /* write through cachine for pt? 
        pt -> pt_pcd     = 0  ;       //     /* cache disable for this pt?   
        pt -> pt_acc     = 0  ;       //     /* page table was accessed? 
        pt -> pt_dirty   = 0  ;       // /* must be zero         
        pt -> pt_mbz     = 0  ;       // /* four MB pages?       
        pt -> pt_global  = 0  ;               // /* global (ignored)     
        pt -> pt_avail   = 0  ;               // /* for programmer's use     
        pt -> pt_base    = page_number  ; //     /* location of page table?  
}

unsigned long tmp ;
void write_CR( unsigned long cr , int idx){
    
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
        return ; 
    }
    asm("popl %eax");

  restore(mask);
  return ;
} 

unsigned long read_CR(unsigned long cr , int idx) {
  
    intmask		mask;		/* Saved interrupt mask		*/
    mask = disable();
    unsigned long local_tmp;


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
        return -1 ;
    }
    asm("movl %eax, tmp");
    asm("popl %eax");

    local_tmp = tmp;
    //cr = local_tmp ; 

    restore(mask);
    return local_tmp ;
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

struct ivent*   fifo_head  ;
struct ivent*   fifo_tail  ;

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

	resume (
	   vcreate((void *)main, INITSTK, 0 , INITPRIO, "Main process", 0,
           NULL));
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
    //1. set_IVTab() ; 
    reset_ivtentry() ;
    // null_pd , global_pt , device_pt 
    set_ivtentry_pd_pt( 0 , 0 , PAGE_DIR ) ;
    for( uint32 i = 1 ; i < 6 ; i++){
        set_ivtentry_pd_pt( i , 0 , PAGE_TAB ) ;
    }
    //2. set_BSmapTab() ; 
    reset_BSmapTab() ; 

    //3. set Null process's paging members.
	struct	procent	*nullptr;		/* Ptr to process table entry	*/
	nullptr = &proctab[NULLPROC];
    nullptr -> PDBR = Cal_Addr( 0 ) ; 
    //4. assign Null process's page directories to global page table
    pd_t* null_pd = (pd_t*)Cal_Addr( 0 ) ;
    set_pd_t( null_pd++ , 1 + 1024 ) ;
    set_pd_t( null_pd++ , 2 + 1024 ) ;
    set_pd_t( null_pd++ , 3 + 1024 ) ;
    set_pd_t( null_pd   , 4 + 1024 ) ; 
    // 5. set gloabl page tables
    pt_t* g_pt_t = (pt_t*)Cal_Addr( 1 ) ; 
    for( int p = 0 ; p < 4 ; p ++)
        for( int i = 0 ; i < 1024 ; i ++ )
            set_pt_t( g_pt_t++ , p * 1024 + i ) ; 
    // 6. set device memory pd and pt
    pd_t* device_pd_t = (pd_t*)( null_pd + 4 * 576 ) ;
    set_pd_t( device_pd_t ,  5 + 1024 ) ;
    
    pt_t* device_pt_t = (pt_t*)Cal_Addr( 576 ) ; 
    for( int i = 0 ; i < 1024 ; i ++)
        set_pt_t( device_pt_t++ , 576*1024 + i ) ; 
    
    // 7. set CR3 / PDBR 
    write_CR( nullptr -> PDBR , 3 ) ;
    check_CR3 = nullptr -> PDBR ;  
    // 8. install ISR
    set_evec( 14 , (uint32) pagefault ) ; 

    // 9.enable page table 
    unsigned long cr0 = 0 ; 
    cr0 = read_CR( cr0 , 0 ) ;
    cr0 = cr0 | (1 << 31 ) | 1 ;
    write_CR( cr0 , 0) ; 
    unsigned long cr4 = 0 ;
    cr4 = read_CR( cr4 , 4  ) ;
    cr4 = cr4 & (~(1 << 5 ) )  ;
    write_CR( cr4 , 4) ;
#endif
    //
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
    for( uint32 i = 0 ; i < NFRAMES ; i++){
        if( ivptab[i].valid == NULL_PAGE ){
            ivptab[i].valid = purpose ; 
            ivptab[i].pid = pid ;
            if( purpose == PAGE ){
                ivptab[i].vpage_num = vpn ; 
                
                fifo_tail -> fifo_prev = &ivptab[i] ; 
                ivptab[i].fifo_next = fifo_tail ; 
                ivptab[i].fifo_prev = NULL            ; 
                fifo_tail = &ivptab[i]         ;  
            }    
            return i ; 
        }
    }
    
    uint32 ret = fifo_head->frame_number ; 
    fifo_head  = fifo_head->fifo_prev ; 
    fifo_head -> fifo_next = NULL ; 
   
    uint32 r_pid = ivptab[ret].pid ; 
    uint32 r_vpn = ivptab[ret].vpage_num ;

    uint32 r_vaddr = r_vpn * PAGE_SIZE ; 
    uint32 r_p = r_vaddr >> 22 ; 
    uint32 r_q = r_vaddr >> 12 & 0x00000cff ;  
   

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
    
    if( r_pt->pt_dirty == 1 ){
        uint32 bstore , p_offset ; 
        get_store_offset( r_pid , r_vaddr , &bstore , &p_offset ) ; 
        write_bs( (char *)( ( ret + 1024 ) * PAGE_SIZE ) , bstore , p_offset ); 
    }
    
    r_pt -> pt_pres = 0 ;
    if( currpid == r_pid ) 
        asm volatile("invlpg (%0)" ::"r" (r_vaddr) : "memory");
    
    ivptab[ r_pd->pd_base - 1024].ref_count -- ; 
    if ( ivptab[ r_pd->pd_base - 1024].ref_count == 0 ){
        r_pd -> pd_pres = 0 ; 
        ivptab[r_pd->pd_base - 1024].valid = NULL_PAGE ; 
    }
    
    ivptab[ret].valid = purpose ; 
    ivptab[ret].pid = pid ;
    if( purpose == PAGE ){
        ivptab[ret].vpage_num = vpn ; 
    
        fifo_tail   -> fifo_prev = &ivptab[ret] ; 
        ivptab[ret].fifo_next = fifo_tail ; 
        ivptab[ret].fifo_prev = NULL      ; 
        fifo_tail = &ivptab[ret]         ;  
    }  
    return ret ; 
}

void myPageFault(){

	
    intmask	mask;			/* Saved interrupt mask		*/
    uint64 pf_addr = 0 ;
    uint32 v_pn , v_pd_idx , v_pt_idx ;
    //uint32 store, p_offset , free_frame_num ;
    struct procent* currPtr ; 
    pd_t* curr_pd    ; 
    pt_t* curr_pt    ; 

    mask = disable();

    currPtr = &proctab[currpid] ; 
    pf_addr = read_CR( pf_addr , 2 ) ; // read faulted address ; 
    //check if a is valid 
    if( pf_addr < currPtr->vmemlist.vaddr || pf_addr >= currPtr->vmaxheap.vaddr ){
        kprintf("faulted address is not valid \n") ; 
        // TODO kill the process ??? 
    }
    
    v_pn = pf_addr >> 12 ; 
    v_pd_idx = v_pn >> 10 ;
    v_pt_idx = v_pn & (0x000003FF) ; 
    
    curr_pd = (pd_t*)( (uint32)(currPtr -> PDBR) + 4 * v_pd_idx ) ; 
    if( curr_pd -> pd_pres == 0 ){ // the page table does not exist 
        create_pagetable( curr_pd ) ;  
        curr_pt = (pt_t*)( Cal_Addr( curr_pd->pd_base - 1024) + 4 * v_pt_idx  ) ; 
    }else{
        curr_pt = (pt_t*)( Cal_Addr( curr_pd->pd_base - 1024) + 4 * v_pt_idx  ) ; 
    }
    
    uint32 bstore , p_offset ; 
    get_store_offset( currpid , pf_addr , &bstore , &p_offset ) ; 
    uint32 free_frame_num = get_free_frame_number( currpid , v_pn , PAGE ) ; 
    read_bs( (char*)( ( free_frame_num + 1024) * PAGE_SIZE ) , bstore , p_offset ) ; 
     
    
    set_pt_t( curr_pt , free_frame_num + 1024 ) ; 
    ivptab[ curr_pd->pd_base-1024 ].ref_count ++ ; 

    restore(mask);
    //kprintf("entering page fault \n") ; 
    return ;  
}

void create_pagetable(pd_t* pd){
    uint32 free_frame_num = get_free_frame_number( currpid , 0 , PAGE_TAB ) ;
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
            if( vpn - bsmap[i].start_vpn< bsmap[i].num_pages ){
                *store = i ; 
                *offset = vpn - bsmap[i].start_vpn ; 
                return ; 
            }
        }
    }
    //kprintf("BSmap Finding error \n" ) ; 
}


