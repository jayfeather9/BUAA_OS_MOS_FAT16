#include <asm/asm.h>
#include <env.h>
#include <kclock.h>
#include <pmap.h>
#include <printk.h>
#include <trap.h>
#include "../../fs/serv.h"
#include "../../fs/fat.h"

int main() {
	debugf("test begin\n");	
	
	fat_init();
	// struct FatDisk *fdk = get_fat_disk();
	// struct FatBPB *fbpb = get_fat_BPB();

	// debug_print_fatBPB();
	// debug_print_fatDisk();

	// unsigned char *path = "fs/fs.c";
	// struct FatShortDir pdir, pfile;
	// debugf("walking with rt val = %d\n", walk_path_fat(path, &pdir, &pfile));
	// unsigned char buf[FAT_MAX_FILE_SIZE];
	// struct FatShortDir dirs[32];
	// debugf("clus = %d %d\n", pdir.FstClusLO, pfile.FstClusLO);
	// debugf("name = %s %s\n", pdir.Name, pfile.Name);
	// debugf("reading with rt val = %d\n", read_dir(pdir.FstClusLO, buf, dirs));
	// debug_list_dir_contents(buf, dirs);

	// fat_user_init();
	// debug_print_fspace();debugf("\n");
	// alloc_fat_file_space(5, 12, 0);
	// debug_print_fspace();debugf("\n");
	// alloc_fat_file_space(4, 12, 0);
	// debug_print_fspace();debugf("\n");
	// alloc_fat_file_space(3, 16384, 0);
	// debug_print_fspace();debugf("\n");
	// free_clus(4);
	// debug_print_fspace();debugf("\n");
	// alloc_fat_file_space(2, 4097, 0);
	// alloc_fat_file_space(1, 12, 0);
	// debug_print_fspace();debugf("\n");
	// free_clus(3);
	// debug_print_fspace();debugf("\n");
	// free_clus(2);
	// debug_print_fspace();debugf("\n");
	// free_clus(1);
	// free_clus(5);
	// debug_print_fspace();debugf("\n");


	return 0;
}
