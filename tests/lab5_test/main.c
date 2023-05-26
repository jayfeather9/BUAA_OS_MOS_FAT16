
#include <lib.h>
#include "../../fs/serv.h"
#include "../../fs/fat.h"

int main() {
	debugf("test begin\n");	
	
	fat_init();
	struct FatDisk *fdk = get_fat_disk();
	struct FatBPB *fbpb = get_fat_BPB();
	debug_print_fatsec(fdk->FirstRootDirSecNum);

	debug_print_fatBPB();
	debug_print_fatDisk();

	// debug_print_fatsec(fbpb->RsvdSecCnt);

	// read_root();
	
	unsigned char buf[32767];
	struct FatShortDir dirs[32];
	read_dir(0, buf, dirs);
	debug_print_file_as_dir_entry(0);
	debug_list_dir_contents(buf, dirs);
	// debugf("freeing rt val = %d\n", free_dir(2, "FS.C"));
	// read_dir(2, buf, dirs);
	// debug_list_dir_contents(buf, dirs);

	// read & write test
	/*
	read_fat_clusters(1614, buf, 100);
	unsigned char *bufp = buf;
	for (int i = 0; i < 100; i++) debugf("%c", *bufp++);
	debugf("\n\n");
	write_fat_clusters(1614, "OHHHHHHHHH", 11);
	read_fat_clusters(1614, buf, 100);
	bufp = buf;
	for (int i = 0; i < 100; i++) debugf("%c", *bufp++);
	debugf("\n\n");
	*/

	return 0;
}
