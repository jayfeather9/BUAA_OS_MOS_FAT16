#include <asm/asm.h>
#include <env.h>
#include <kclock.h>
#include <pmap.h>
#include <printk.h>
#include <trap.h>

// When build with 'make test lab=?_?', we will replace your 'mips_init' with a generated one from
// 'tests/lab?_?'.
#ifdef MOS_INIT_OVERRIDDEN
#include <generated/init_override.h>
#else

void mips_init() {
	printk("init.c:\tmips_init() is called\n");

	// lab2:
	mips_detect_memory();
	mips_vm_init();
	page_init();

	// lab3:
	env_init();

	printk("finished env_init()\n");

	// lab3:
	ENV_CREATE_PRIORITY(user_bare_loop, 1);
	printk("Finished create env 1\n");
	
	ENV_CREATE_PRIORITY(user_bare_loop, 2);
	printk("Finished create env 2\n");

	// lab4:
	// ENV_CREATE(user_tltest);
	// ENV_CREATE(user_fktest);
	// ENV_CREATE(user_pingpong);

	// lab6:
	// ENV_CREATE(user_icode);  // This must be the first env!

	// lab5:
	// ENV_CREATE(user_fstest);
	// ENV_CREATE(fs_serv);  // This must be the second env!
	// ENV_CREATE(user_devtst);

	// lab3:
	// kclock_init();
	// printk("Finished kclock_init\n");
	// enable_irq();
	// printk("Finished enable_irq\n");
	while (1) {
	}
}

#endif
