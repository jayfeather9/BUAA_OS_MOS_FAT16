
#include <lib.h>
#include "../../fs/serv.h"
#include "../../fs/fat.h"

int main() {
	debugf("test begin\n");	
	
	fat_init();
    debug_print_cluster_data(0);
    debug_print_cluster_data(1);
    debug_print_cluster_data(2);
    debug_print_cluster_data(3);

	return 0;
}
