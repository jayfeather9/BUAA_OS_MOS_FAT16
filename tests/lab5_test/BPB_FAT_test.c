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
	// set_fat_entry(2, 0x5f);
	// debug_print_fat_entry(2);
	debug_print_fatsec(4);
	debug_print_fatsec(72);

	uint32_t clusst, clus, clus2st, clus2, next_entry;

	debugf("alloc 1 cluster\n");
	user_assert(0 == alloc_fat_clusters(&clus, 1));
	debug_print_fat_entry(clus);
	clusst = clus;

	debugf("alloc 8 clusters\n");
	user_assert(0 == alloc_fat_clusters(&clus2, 8));
	clus2st = clus2;
	next_entry = clus2;
	while (clus2 != 0xFFFF) {
		debug_print_fat_entry(clus2);
		get_fat_entry(clus2, &next_entry);
		clus2 = next_entry;
	}
	debug_print_fatsec(4);
	debug_print_fatsec(72);

	clus = clusst;
	debugf("expanding 7 clusters to prev 1 cluster\n");
	expand_fat_clusters(&clus, 7);
	next_entry = clus;
	while (clus != 0xFFFF) {
		debug_print_fat_entry(clus);
		get_fat_entry(clus, &next_entry);
		clus = next_entry;
	}

	clus = clusst;
	clus2 = clus2st;
	debugf("free the clusters\n");
	free_fat_clusters(clus);
	free_fat_clusters(clus2);
	for (int i = 0; i < 20; i++) {
		debug_print_fat_entry(i);
	}
	return 0;
}
