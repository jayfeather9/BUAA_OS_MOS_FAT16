#include <asm/asm.h>
#include <env.h>
#include <kclock.h>
#include <pmap.h>
#include <printk.h>
#include <trap.h>
#include "../../fs/fatserv.h"

int main() {
	debugf("test begin\n");	
	// debug_print_fatsec(0);
	fat_fs_init();
	// debug_print_fatBPB();
	// debug_print_fatDisk();
	// debug_print_file_as_dir_entry((char *)FATROOTVA);
	// struct FATDIRENT *ent = (struct FATDIRENT *)FATROOTVA;
	// ent += 4;
	// debug_print_short_dir(ent);
	// u_int clus;
	// debugf("get cluster rt val = %d\n", fat_file_get_cluster_by_order(ent, 5, &clus, 1));
	// debugf("returned from get cluster clus = %u\n", clus);
	// char buf[FAT_MAX_CLUS_SIZE];
	// read_disk_fat_cluster(clus, (unsigned char *)buf);
	// struct FATDIRENT *ent2 = (struct FATDIRENT *)buf;
	// debug_print_short_dir(ent2);

	// struct FATDIRENT *ent = (struct FATDIRENT *)FATROOTVA;
	// ent += 22;
	// debug_print_short_dir(ent);
	// fat_file_clear_clus(ent, 1);
	// debug_print_short_dir(ent);
	// fat_write_clus(0);

	struct FATDIRENT *ent, *pent;
	// debugf("dir lookup rt val = %d\n", fat_dir_lookup(fat_get_root(), "test", &ent));
	// pent = ent;
	// debugf("starting read clus\n");
	// fat_read_clus(pent->DIR_FstClusLO, 0, 0);
	// debugf("end reading clus\n");
	// // debugf("dir lookup rt val = %d\n", fat_dir_lookup(pent, "include.mk", &ent));
	// // debug_print_short_dir(ent);
	// struct FATDIRENT *files;
	// fat_dir_alloc_files(pent, &files, 64);
	// for (struct FATDIRENT *ient = files; ient - files < 64; ient++) {
	// 	ient->DIR_Name[0] = 'a';
	// }
	void *va;
	// u_int clus;
	// get_fat_entry(pent->DIR_FstClusLO, &clus);
	// is_clus_mapped(clus, (uint32_t *)&va);
	// debug_print_file_as_dir_entry(va);
	// fat_dir_alloc_files(pent, &files, 20);
	// for (struct FATDIRENT *ient = files; ient - files < 20; ient++) {
	// 	ient->DIR_Name[0] = 'b';
	// }
	// is_clus_mapped(pent->DIR_FstClusLO, (uint32_t *)&va);
	// debug_print_file_as_dir_entry(va);

	// debugf("walk path rt val = %d\n", fat_walk_path("/fs/../fs/../fs/fs.c", &pent, &ent, 0));
	// debug_print_short_dir(pent, 0);
	// debug_print_short_dir(ent, 0);

	// debugf("create file rt val = %d\n", fat_file_create("/long_name_good.doog", &ent, &pent));
	// ent->DIR_Attr = FAT_ATTR_DIRECTORY;
	// u_int clus;
	// alloc_fat_cluster_entries(&clus, 1);
	// ent->DIR_FstClusLO = clus;
	// ent->DIR_FileSize = 0;
	// fat_write_clus(pent->DIR_FstClusLO);
	// is_clus_mapped(pent->DIR_FstClusLO, &va);
	// debug_print_file_as_dir_entry(va);

	// debugf("walk path rt val = %d\n", fat_walk_path("/long_file.txt", &pent, &ent, 0));
	// uint32_t clus_num;
	// query_fat_clusters(ent->DIR_FstClusLO, &clus_num);
	// debugf("get cluster num = %u\n", clus_num);
	// fat_file_truncate(ent, 1200000);
	// query_fat_clusters(ent->DIR_FstClusLO, &clus_num);
	// debugf("get cluster num = %u\n", clus_num);
	// fat_write_clus(pent->DIR_FstClusLO);
	// fat_file_truncate(ent, 5);
	// query_fat_clusters(ent->DIR_FstClusLO, &clus_num);
	// debugf("get cluster num = %u\n", clus_num);
	// // fat_write_clus(pent->DIR_FstClusLO);

	// debugf("file remove rt val = %d\n", fat_file_remove("/long_file.txt"));
	// debugf("walk path rt val = %d\n", fat_walk_path("/a.abcde", &pent, &ent, 0, 0));
	// is_clus_mapped(pent->DIR_FstClusLO, &va);
	// debug_print_file_as_dir_entry(va);
	return 0;
}
