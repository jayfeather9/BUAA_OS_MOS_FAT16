#include <lib.h>
#include "../../fs/serv.h"
#include "../../fs/fat.h"

int main() {
	debugf("test begin\n");	
	
	fat_init();
	debug_print_fatBPB();
	debug_print_fatDisk();
	debug_print_fat_entry(0);
	debug_print_fat_entry(1);
	debug_print_fat_entry(2);
	set_fat_entry(2, 0x5f);
	debug_print_fat_entry(2);
	debug_print_fatsec(4);

	return 0;
}
