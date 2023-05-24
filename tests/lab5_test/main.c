
#include <lib.h>
#include "../../fs/serv.h"
#include "../../fs/fat.h"

int main() {
	debugf("test begin\n");	
	
	fat_init();
  struct FatDisk *fdk = get_fat_disk();
  debug_print_fatsec(fdk->FirstRootDirSecNum);

	debug_print_fatBPB();
	debug_print_fatDisk();
	read_dir();
	return 0;
}
