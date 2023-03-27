#include <asm/asm.h>

LEAF(tlb_out) /* starting macro for function definition */
.set noreorder /* disable assembler instruction reordering */
	mfc0    t0, CP0_ENTRYHI /* save the original value of EntryHi */
	mtc0    a0, CP0_ENTRYHI /* put our KEY into EntryHi */
	nop
	/* Step 1: Use 'tlbp' to probe TLB entry */
	/* Exercise 2.8: Your code here. (1/2) */

	tlbp /* use our KEY to search if there's corresponding item in TLB */

	nop
	/* Step 2: Fetch the probe result from CP0.Index */
	mfc0    t1, CP0_INDEX /* check if hit the TLB item by checking INDEX */
.set reorder /* enable assembler instruction reordering */
	bltz    t1, NO_SUCH_ENTRY /* "if hit then", or jump to end if not hit */
.set noreorder /* disable assembler instruction reordering */
	mtc0    zero, CP0_ENTRYHI /* clear EntryHi */
	mtc0    zero, CP0_ENTRYLO0 /* clear EntryLo */
	nop
	/* Step 3: Use 'tlbwi' to write CP0.EntryHi/Lo into TLB at CP0.Index  */
	/* Exercise 2.8: Your code here. (2/2) */

	tlbwi /* write the cleared EntryHi & EntryLo into corresponding item in TLB */

.set reorder /* enable assembler instruction reordering */

NO_SUCH_ENTRY:
	mtc0    t0, CP0_ENTRYHI /* restore original EntryHi value */
	j       ra /* return */
END(tlb_out) /* ending macro for function end */

.data
tlb_refill_ra:
.word 0
.text
NESTED(do_tlb_refill, 0, zero)
	mfc0    a0, CP0_BADVADDR
	mfc0    a1, CP0_ENTRYHI
	srl     a1, a1, 6
	andi    a1, a1, 0b111111
	sw      ra, tlb_refill_ra
	jal     _do_tlb_refill
	lw      ra, tlb_refill_ra
	mtc0    v0, CP0_ENTRYLO0
	// See <IDT R30xx Family Software Reference Manual> Chapter 6-8
	nop
	/* Hint: use 'tlbwr' to write CP0.EntryHi/Lo into a random tlb entry. */
	/* Exercise 2.10: Your code here. */

	tlbwr

	jr      ra
END(do_tlb_refill)
