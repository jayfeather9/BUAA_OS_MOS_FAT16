
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
	
	/*
	unsigned char buf[32767];
	struct FatShortDir dirs[32], pdir;
	read_dir(0, buf, dirs);
	pdir = dirs[9];
	debugf("looking into dir name = ");
	for (int i = 0; i < 11; i++) debugf("%c", pdir.Name[i]);
	debugf("\n");	
	// debug_print_file_as_dir_entry(2);
	debug_list_dir_contents(buf, dirs);
	read_dir(pdir.FstClusLO, buf, dirs);
	debug_list_dir_contents(buf, dirs);
	unsigned char content[50] = "hello\nHi!\n";
	debugf("creating file with rt val = %d\n", create_file(&pdir, "testfile.txt", content, 50, 0));
	*/
	/*
	debugf("creating file with rt val = %d\n", create_file(&pdir, "good.py", content, 50, 0));
	debugf("creating file with rt val = %d\n", create_file(&pdir, "bad.txtovo", content, 50, 0));
	debugf("creating file with rt val = %d\n", create_file(&pdir, "badbbbbbbbbbbbbc.txt", content, 50, 0));
	debugf("creating file with rt val = %d\n", create_file(&pdir, "badbbbbbbbbbbbbc.txt", content, 50, 0));
	debugf("creating file with rt val = %d\n", create_file(&pdir, "badbbbbbbbbbbbbc.txt", content, 50, 0));
	*/
/*
	read_dir(pdir.FstClusLO, buf, dirs);
	debug_list_dir_contents(buf, dirs);
	debugf("reading file with rt val = %d\n", read_file(&pdir, "testfile.txt", buf, 5));
	buf[5] = '\0';
	debugf("read content: [%s]\n", buf);
	
	read_dir(pdir.FstClusLO, buf, dirs);
	debug_list_dir_contents(buf, dirs);
	unsigned char wrt_content[16384];
	for (int i = 0; i < 16383; i++) wrt_content[i] = 'a' + (i + i*i) % 26;
	wrt_content[16383] = 0;
	debugf("writing file with rt val = %d\n", write_file(&pdir, "testfile.txt", wrt_content, 16384));
	read_dir(pdir.FstClusLO, buf, dirs);
	debug_list_dir_contents(buf, dirs);
	*/


	// debug_print_file_as_dir_entry(pdir.FstClusLO);
	// u_int sec, us, yr, mnth, dy, hr, mn, s;
	// sec = get_time(&us);
	// get_all_time(sec, &yr, &mnth, &dy, &hr, &mn, &s);
	// debugf("%u %u %u %u %u %u %u %u\n", sec, us, yr, mnth, dy, hr, mn, s);
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
