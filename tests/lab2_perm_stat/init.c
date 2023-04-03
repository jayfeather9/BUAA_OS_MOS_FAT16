#include <mmu.h>
#include <pmap.h>
#include <print.h>

void test_perm_stat() {
	struct Page *p;
	assert(page_alloc(&p) == 0);
	Pde *pgdir = (Pde *)page2kva(p);
	u_int va[4] = {UTEXT, UTEXT + BY2PG, UTEXT + 1024 * BY2PG, UTEXT + 1025 * BY2PG};
	u_int perm[4] = {PTE_V | PTE_D, PTE_V | PTE_D | PTE_G, PTE_V | PTE_D | PTE_G,
			 PTE_V | PTE_G};
	struct Page *pp, *pp2, *pp3, *pp4;
	for (int i = 0; i < 4; i++){
		printk("va = %u, PDX = %u, PTX = %u\n", va[i], PDX(va[i]), PTX(va[i]));
	}
	assert(page_alloc(&pp) == 0);
	assert(page_alloc(&pp2) == 0);
	assert(page_alloc(&pp3) == 0);
	assert(page_alloc(&pp4) == 0);
	for (unsigned int nnn = 0; nnn < 5; nnn++)
	assert(page_insert(pgdir, 0, pp, 0xf0000000 + (nnn - 3)*BY2PG, perm[0]) == 0);
	//assert(page_insert(pgdir, 0, pp2, va[1], perm[1]) == 0);
	//assert(page_insert(pgdir, 0, pp3, va[2], perm[2]) == 0);
	//assert(page_insert(pgdir, 0, pp, va[3], perm[3]) == 0);
	int r = page_perm_stat(pgdir, pp, PTE_D);
	printk("r = %d\n", r);
	assert(r == 1);
	printk("test_perm_stat succeeded!\n");
}

void mips_init() {
	mips_detect_memory();
	mips_vm_init();
	page_init();
	test_perm_stat();
	halt();
}
