#include "fatserv.h"

struct FatOpen {
	struct FATDIRENT *o_file; // mapped descriptor for open file
	struct FATDIRENT *o_dir;
	u_int o_fileid;	     // file id
	int o_mode;	     // open mode
	struct Fatfd *o_ff; // va of filefd page
};

// Max number of open files in the file system at once
#define MAXOPEN 1024
#define FILEVA 0x60000000

// initialize to force into data section
struct FatOpen opentab[MAXOPEN] = {{0, 0, 0, 1}};

// Virtual address at which to receive page mappings containing client requests.
#define REQVA 0x0ffff000

// Overview:
//  Initialize file system server process.
void fat_serve_init(void) {
	int i;
	u_int va;

	// Set virtual address to map.
	va = FILEVA;

	// Initial array opentab.
	for (i = 0; i < MAXOPEN; i++) {
		opentab[i].o_fileid = i;
		opentab[i].o_ff = (struct Fatfd *)va;
		va += BY2PG;
	}
}

// Overview:
//  Allocate an open file.
int fat_open_alloc(struct FatOpen **o) {
	int i, r;

	// Find an available open-file table entry
	for (i = 0; i < MAXOPEN; i++) {
		switch (pageref(opentab[i].o_ff)) {
		case 0:
			if ((r = syscall_mem_alloc(0, opentab[i].o_ff, PTE_D | PTE_LIBRARY)) < 0) {
				return r;
			}
		case 1:
			opentab[i].o_fileid += MAXOPEN;
			*o = &opentab[i];
			memset((void *)opentab[i].o_ff, 0, BY2PG);
			return (*o)->o_fileid;
		}
	}

	return -E_FAT_MAX_OPEN;
}

// Overview:
//  Look up an open file for envid.
int open_lookup(u_int envid, u_int fileid, struct FatOpen **po) {
	struct FatOpen *o;

	o = &opentab[fileid % MAXOPEN];

	if (pageref(o->o_ff) == 1 || o->o_fileid != fileid) {
		return -E_FAT_INVAL;
	}

	*po = o;
	return 0;
}

int main() {
	return 0;
}