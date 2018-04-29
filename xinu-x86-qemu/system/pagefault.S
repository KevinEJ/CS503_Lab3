		.text
error_code:
        .long 0
		.globl	pagefault,error_code		# Clock interrupt dispatcher

pagefault:
	//cli             /* clear all intrs */
	popl error_code /* store error code in global memory and remove */
	pushal          /* save all register */
	call myPageFault 
	popal           /*; restore all register */
	//sti  
	iret  	        /*# Return from interrupt   */  
