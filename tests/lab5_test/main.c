#include <lib.h>
#include "../../fs/serv.h"
#include "../../fs/fat.h"

int main() {
	debugf("test begin\n");	
	
	fat_init();
	debug_print_fatBPB();
	debug_print_fatsec(1);

	return 0;
}
