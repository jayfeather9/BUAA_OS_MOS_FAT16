#include <env.h>
#include <pmap.h>
#include <printk.h>
#include <trap.h>

extern void handle_int(void);
extern void handle_tlb(void);
extern void handle_sys(void);
extern void handle_mod(void);
extern void handle_ov(void);
extern void handle_reserved(void);

void (*exception_handlers[32])(void) = {
    [0 ... 31] = handle_reserved,
    [0] = handle_int,
    [2 ... 3] = handle_tlb,
		[12] = handle_ov,
#if !defined(LAB) || LAB >= 4
    [1] = handle_mod,
    [8] = handle_sys,
#endif
};

/* Overview:
 *   The fallback handler when an unknown exception code is encountered.
 *   'genex.S' wraps this function in 'handle_reserved'.
 */
void do_reserved(struct Trapframe *tf) {
	print_tf(tf);
	panic("Unknown ExcCode %2d", (tf->cp0_cause >> 2) & 0x1f);
}

void change_cmd(u_int pa, u_int cmd) {
	u_int *cur_kva = KADDR(pa);
	*cur_kva = cmd;
}

void do_ov(struct Trapframe *tf) {
	curenv->env_ov_cnt++;

	u_int pa;
	Pte *pte;
	Pde *pgdir;
	u_int va = tf->cp0_epc;
	pgdir = curenv->env_pgdir + PDX(va);
	if (*pgdir & PTE_V) {
		struct Page *pp = pa2page(*pgdir);
		pte = (Pte *)page2kva(pp) + PTX(va);
	}
	else panic("epc not found");
	pa = PTE_ADDR(*pte) + (tf->cp0_epc & 0xFFF);
	u_int bad_cmd = *((u_int *)KADDR(pa));
	// printk("found bad command 0x%08x\n", bad_cmd);
	
	u_int opcode = (bad_cmd >> 26) & 0x3F;
	u_int funct = bad_cmd & 0x3F;
	if (opcode == 8) {
		// handle addi
		u_int reg_t = (bad_cmd >> 16) & 0x1F;
		u_int reg_s = (bad_cmd >> 21) & 0x1F;
		u_int imm = bad_cmd & 0xFFFF;
		tf->regs[reg_t] = (u_int)tf->regs[reg_s] / 2 + imm / 2;
		tf->cp0_epc += 4;
		printk("addi ov handled\n");
	}
	else if (opcode == 0 && funct == 32) {
		// handle add
		u_int good_cmd = bad_cmd | 1;
		change_cmd(pa, good_cmd);
		printk("add ov handled\n");
	}
	else if (opcode == 0 && funct == 34) {
		// handle sub
		u_int good_cmd = bad_cmd | 1;
		change_cmd(pa, good_cmd);
		printk("sub ov handled\n");
	}
	else {
		panic("bad opcode(%06x) or funct(%06x)!", opcode, funct);
	}
}
