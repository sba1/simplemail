/* -------------------- HookEntry ---------------------------*/

	.globl     hookEntry
	.type      hookEntry, @function

		.section .data

hookEntry:
	.short     0x4ef8                /* JMP.w */
	.short     0                     /* Indicate switch */
	.short     1                     /* Trap type */
	.long      HookEntryPPC
	/* Register mapping */
	.byte      4
	.byte      1,60          
	.byte      3,32
	.byte	4,40
	.byte	5,36
	.align	4

		.section .text
	.align     2
HookEntryPPC:
		addi       r12, r1, -16          /* Calculate stackframe size */
		rlwinm     r12, r12, 0, 0, 27    /* Align it */
		stw        r1, 0(r12)            /* Store backchain pointer */
		mr         r1, r12               /* Set real stack pointer */

		stw		r11,12(r1)				/* Store Enter68kQuick vector */

		lwz		r12,12(r3)		/* Get the sub entry */
		mtlr	r12
		blrl
		
		lwz		r11,12(r1)
		mtlr	r11
	
		lwz     r1, 0(r1)				/* Cleanup stack frame */

		blrl							/* Return to emulation */
		.long      HookExit
		.byte      0                     /* Flags */
		.byte      2                     /* Two registers (a7 and d0) */
		.byte      1                     /* Map r1 */
		.byte      60                    /* to A7 */
		.byte      3                     /* Map r3 */
		.byte      0                     /* to D0 */
		.align     4
		.section .data
HookExit:		
	.short     0x4e75                /* RTS */

