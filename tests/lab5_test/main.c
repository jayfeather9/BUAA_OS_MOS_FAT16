
#include <lib.h>
#include "../../fs/serv.h"
#include "../../fs/fat.h"

int main() {
	debugf("test begin\n");	
	
	fat_init();
	struct FatDisk *fdk = get_fat_disk();
	struct FatBPB *fbpb = get_fat_BPB();

	debug_print_fatBPB();
	debug_print_fatDisk();

	unsigned char *path = "fs/fs.c";
	struct FatShortDir pdir, pfile;
	debugf("walking with rt val = %d\n", walk_path_fat(path, &pdir, &pfile));
	unsigned char buf[FAT_MAX_FILE_SIZE];
	struct FatShortDir dirs[32];
	debugf("clus = %d %d\n", pdir.FstClusLO, pfile.FstClusLO);
	debugf("name = %s %s\n", pdir.Name, pfile.Name);
	debugf("reading with rt val = %d\n", read_dir(pdir.FstClusLO, buf, dirs));
	debug_list_dir_contents(buf, dirs);

	return 0;
}
