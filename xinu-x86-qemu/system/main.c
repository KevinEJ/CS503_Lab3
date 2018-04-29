#include <xinu.h>

extern void page_policy_test(void);


uint32 phy_get_test_value(uint32 *addr) {
  static uint32 pv1 = 0x12345678;
  static uint32 pv2 = 0xdeadbeef;
  return (uint32)addr + pv1 + ((uint32)addr * pv2);
}

void phy_mem_test(void){
  uint32 npages = 5;
  uint32 nbytes = npages * PAGE_SIZE;

  kprintf("get mem  \n") ; 
  char *mem = getmem(nbytes);
  kprintf("mem %x \n" , (uint32)mem ) ; 

  // Write data
  kprintf("write  \n") ; 
  for (uint32 i = 0; i<npages; i++) {
    uint32 *p = (uint32*)(mem + (i * PAGE_SIZE));

    kprintf("Write Iteration [%3d] at 0x%08x\n", i, p);
    for (uint32 j=0; j<PAGE_SIZE; j=j+4) {
      uint32 v = phy_get_test_value(p);
      *p++ = v;
    }

    sleepms(20); // to make it slower
  }

  // Check the data was truly written
  kprintf("check  \n") ; 
  for (uint32 i = 0; i<npages; i++) {
    uint32 *p = (uint32*)(mem + (i * PAGE_SIZE));
    kprintf("Check Iteration [%3d] at 0x%08x\n", i, p);
    for (uint32 j=0; j<PAGE_SIZE; j=j+4) {
      uint32 v = phy_get_test_value(p);
      ASSERT(*p++ == v);
    }

    sleepms(20); // to make it slower
  }


  vfreemem(mem, nbytes); 
}

process	main(void)
{
  kprintf("srpolicy \n") ; 
  phy_mem_test();
  
  kprintf("srpolicy \n") ; 
  srpolicy(FIFO);

  /* Start the network */
  /* DO NOT REMOVE OR COMMENT BELOW */
#ifndef QEMU
  kprintf("netstart \n") ; 
  netstart();
#endif

  /*  TODO. Please ensure that your paging is activated 
      by checking the values from the control register.
  */
  kprintf("psinit \n") ; 
  /* Initialize the page server */
  /* DO NOT REMOVE OR COMMENT THIS CALL */
  psinit();

//  kprintf("page_policy \n") ; 
  page_policy_test();

  return OK;
}
