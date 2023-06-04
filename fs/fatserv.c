#include "serv.h"
#include "fat.h"
#include <fd.h>
#include <fsreq.h>
#include <lib.h>
#include <mmu.h>

struct FatOpen {
	struct FatShortDir o_dir_entry;
	struct FatShortDir o_parent_dir;
	uint32_t o_fileid;
	int o_mode;
	struct FatFilefd *o_fatffd;
};

#define MAXFATOPEN 1024
#define FILEVA 0x60000000

#define FATMAGIC 0xDEADBEEF

struct FatOpen fatopentab[MAXFATOPEN];

#define REQVA 0x0ffff000

void fat_serve_init(void) {
	int i;
	u_int va;

	// Set virtual address to map.
	va = FILEVA;

	// Initial array opentab.
	for (i = 0; i < MAXFATOPEN; i++) {
		fatopentab[i].o_fileid = i;
		fatopentab[i].o_fatffd = (struct FatFilefd *)va;
		va += BY2PG;
	}
}

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
	if ((r = fat_open_alloc(&o)) < 0) {
		ipc_send(envid, r, 0, 0);
	}

	// Open the file.
	if ((r = walk_path_fat((unsigned char *)rq->req_path, &o->o_parent_dir, &o->o_dir_entry)) < 0) {
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

int wrap_fat_remove(char *path) {
	int r;
	struct FatShortDir pdir, dir;
	if ((r = walk_path_fat((unsigned char *)path, &pdir, &dir)) < 0) {
		return r;
	}
	char *p = path;
	while (*p != '\0')p++;
	while (*p != '/' && p > path)p--;
	if (*p == '/')p++;
	if ((r = free_dir(&pdir, (unsigned char *)p)) < 0) {
		debugf("free dir path = %s, rt val = -0x%X\n", p, -r);
		return r;
	}
	return 0;
}

void fat_serve_remove(u_int envid, struct Fsreq_remove *rq) {
	int r;
	r = wrap_fat_remove(rq->req_path);
	ipc_send(envid, r, 0, 0);
}

struct FatBPB tmpinitbuf[2];
void fat_serve_user_init(u_int envid, struct Fsreq_fatinit *rq) {
	// debugf("envid = %d, init done\n", envid);
	if (rq->magic != FATMAGIC) {
		ipc_send(envid, -E_FAT_INVAL, 0, 0);
	}
	struct FatBPB *bpb = get_fat_BPB();
	struct FatDisk *disk = get_fat_disk();
	tmpinitbuf[0] = *bpb;
	// debugf("serv BPB val %u\n", *(char *)tmpinitbuf);
	struct FatDisk *bufd = (struct FatDisk *)&tmpinitbuf[1];
	*bufd = *disk;
	// debugf("envid = %d, init done\n", envid);
	ipc_send(envid, 0, tmpinitbuf, PTE_D | PTE_LIBRARY);
}

void fat_serve_map(u_int envid, struct Fsreq_fatmap *rq) {
	struct FatOpen *pOpen;
	void *va;
	int r;

	if ((r = fat_open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	if ((r = fat_get_va(&pOpen->o_dir_entry, rq->req_pageno, (uint32_t *)&va)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	ipc_send(envid, 0, va, PTE_D | PTE_LIBRARY);
}

void fat_serve_set_size(u_int envid, struct Fsreq_set_size *rq) {
	struct FatOpen *pOpen;
	int r;
	if ((r = fat_open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	if ((r = fat_set_size(pOpen->o_dir_entry, rq->req_size)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	ipc_send(envid, 0, 0, 0);
}

void fat_serve_close(u_int envid, struct Fsreq_close *rq) {
}

void fat_serve_dirty(u_int envid, struct Fsreq_dirty *rq) {

}

void fat_serve_sync(u_int envid) {

}

void fat_serve(void) {
	u_int req, whom, perm;

	for (;;) {
		perm = 0;

		req = ipc_recv(&whom, (void *)REQVA, &perm);
		// debugf("received from %d\n", whom);

		// All requests must contain an argument page
		if (!(perm & PTE_V)) {
			debugf("Invalid request from %08x: no argument page\n", whom);
			continue; // just leave it hanging, waiting for the next request.
		}

		switch (req) {
		case FSREQ_OPEN:
			fat_serve_open(whom, (struct Fsreq_open *)REQVA);
			break;

		case FSREQ_MAP:
			fat_serve_map(whom, (struct Fsreq_fatmap *)REQVA);
			break;

		case FSREQ_SET_SIZE:
			fat_serve_set_size(whom, (struct Fsreq_set_size *)REQVA);
			break;

		case FSREQ_CLOSE:
			fat_serve_close(whom, (struct Fsreq_close *)REQVA);
			break;

		case FSREQ_DIRTY:
			fat_serve_dirty(whom, (struct Fsreq_dirty *)REQVA);
			break;

		case FSREQ_REMOVE:
			fat_serve_remove(whom, (struct Fsreq_remove *)REQVA);
			break;

		case FSREQ_SYNC:
			fat_serve_sync(whom);
			break;
		
		case FSREQ_FATINIT:
			fat_serve_user_init(whom, (struct Fsreq_fatinit *)REQVA);
			break;

		default:
			debugf("FATSERV: Invalid request code %d from %08x\n", whom, req);
			break;
		}

		syscall_mem_unmap(0, (void *)REQVA);
	}
}

int main() {
	fat_serve_init();
	fat_init();
	
	debugf("FAT service init complete, running\n");

	fat_serve();
	
	return 0;
}
