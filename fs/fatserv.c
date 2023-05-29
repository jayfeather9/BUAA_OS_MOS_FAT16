#include "serv.h"
#include "fat.h"
#include <fd.h>
#include <fsreq.h>
#include <lib.h>
#include <mmu.h>

struct FatOpen {
	struct FatShortDir o_dir_entry;
	uint32_t o_fileid;
	int o_mode;
	struct FatFilefd *o_fatffd;
};

#define MAXFATOPEN 1024

struct FatOpen fatopentab[MAXFATOPEN];

// Overview:
//  Allocate an open file.
int fat_open_alloc(struct FatOpen **o) {
	int i, r;

	// Find an available open-file table entry
	for (i = 0; i < MAXFATOPEN; i++) {
		switch (pageref(fatopentab[i].o_fatffd)) {
		case 0:
			if ((r = syscall_mem_alloc(0, fatopentab[i].o_fatffd, PTE_D | PTE_LIBRARY)) < 0) {
				return r;
			}
		case 1:
			fatopentab[i].o_fileid += MAXFATOPEN;
			*o = &fatopentab[i];
			memset((void *)fatopentab[i].o_fatffd, 0, BY2PG);
			return (*o)->o_fileid;
		}
	}

	return -E_FAT_MAX_OPEN;
}

// Overview:
//  Look up an open file for envid.
int fat_open_lookup(u_int envid, u_int fileid, struct FatOpen **po) {
	struct FatOpen *o;

	o = &fatopentab[fileid % MAXFATOPEN];

	if (pageref(o->o_fatffd) == 1 || o->o_fileid != fileid) {
		return -E_FAT_INVAL;
	}

	*po = o;
	return 0;
}

void fat_serve_open(u_int envid, struct Fsreq_open *rq) {
	int r;
	struct FatOpen *o;

	// Find a file id.
	if ((r = open_alloc(&o)) < 0) {
		ipc_send(envid, r, 0, 0);
	}

	// Open the file.
	if ((r = walk_path_fat(rq->req_path, 0, &o->o_dir_entry)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	// Fill out the Filefd structure
	o->o_fatffd->f_dir_entry = o->o_dir_entry;
	o->o_fatffd->f_fileid = o->o_fileid;
	o->o_mode = rq->req_omode;
	o->o_fatffd->f_fd.fd_omode = o->o_mode;
	o->o_fatffd->f_fd.fd_dev_id = devfile.dev_id;

	ipc_send(envid, 0, o->o_fatffd, PTE_D | PTE_LIBRARY);
}

int main() {
	return 0;
}
