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
int fat_open_lookup(u_int envid, u_int fileid, struct FatOpen **po) {
	struct FatOpen *o;

	o = &opentab[fileid % MAXOPEN];

	if (pageref(o->o_ff) == 1 || o->o_fileid != fileid) {
		return -E_FAT_INVAL;
	}

	*po = o;
	return 0;
}

// Serve requests, sending responses back to envid.
// To send a result back, ipc_send(envid, r, 0, 0).
// To include a page, ipc_send(envid, r, srcva, perm).

void fat_serve_open(u_int envid, struct Fatreq_open *rq) {
	struct FATDIRENT *f, *dir;
	struct Fatfd *ff;
	int r;
	struct FatOpen *o;

	// Find a file id.
	if ((r = fat_open_alloc(&o)) < 0) {
		ipc_send(envid, r, 0, 0);
	}

	// Open the file.
	if ((r = fat_file_open(rq->req_path, &f, &dir)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	if (rq->req_omode != O_RDONLY && (f->DIR_Attr & FAT_ATTR_READ_ONLY) == FAT_ATTR_READ_ONLY) {
		user_panic("Opening read only file with incorrect permission");
	}

	// Save the file pointer.
	o->o_file = f;
	o->o_dir = dir;

	// Fill out the Filefd structure
	ff = (struct Fatfd *)o->o_ff;
	ff->f_file = *f;
	ff->f_fileid = o->o_fileid;
	o->o_mode = rq->req_omode;
	ff->f_fd.fd_omode = o->o_mode;
	ff->f_fd.fd_dev_id = devfat.dev_id;

	ipc_send(envid, 0, o->o_ff, PTE_D | PTE_LIBRARY);
}

void fat_serve_map(u_int envid, struct Fatreq_map *rq) {
	struct FatOpen *pOpen;
	u_int fileclno;
	void *va;
	int r;

	if ((r = fat_open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	fileclno = rq->req_offset / BY2BLK;

	// if va is larger than the file size, will alloc more clusters until enough
	if ((r = fat_file_get_clus(pOpen->o_file, fileclno, &va)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	ipc_send(envid, 0, va, PTE_D | PTE_LIBRARY);
}

void fat_serve_set_size(u_int envid, struct Fatreq_set_size *rq) {
	struct FatOpen *pOpen;
	int r;
	if ((r = fat_open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	if ((r = fat_file_set_size(pOpen->o_file, rq->req_size, pOpen->o_dir)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	ipc_send(envid, 0, 0, 0);
}

void fat_serve_close(u_int envid, struct Fatreq_close *rq) {
	struct FatOpen *pOpen;

	int r;

	if ((r = fat_open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	fat_file_close(pOpen->o_file, pOpen->o_dir);
	ipc_send(envid, 0, 0, 0);
}

// Overview:
//  Serve to remove a file specified by the path in `req`.
void fat_serve_remove(u_int envid, struct Fatreq_remove *rq) {
	// Step 1: Remove the file specified in 'rq' using 'file_remove' and store its return value.
	int r;
	r = fat_file_remove(rq->req_path);

	// Step 2: Respond the return value to the requester 'envid' using 'ipc_send'.
	ipc_send(envid, r, 0, 0);
}

void fat_serve_dirty(u_int envid, struct Fatreq_dirty *rq) {
	struct FatOpen *pOpen;
	int r;

	if ((r = fat_open_lookup(envid, rq->req_fileid, &pOpen)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	if ((r = fat_file_dirty(pOpen->o_file, rq->req_offset)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	ipc_send(envid, 0, 0, 0);
}

void fat_serve_sync(u_int envid) {
	fat_fs_sync();
	ipc_send(envid, 0, 0, 0);
}

void fat_serve_create(u_int envid, struct Fatreq_create *rq) {
	struct FATDIRENT *pent;
	int r;

	if ((r = fat_file_create(rq->req_path, &pent, 0)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	ipc_send(envid, 0, 0, 0);
}

void serve(void) {
	u_int req, whom, perm;

	for (;;) {
		perm = 0;

		req = ipc_recv(&whom, (void *)REQVA, &perm);

		// All requests must contain an argument page
		if (!(perm & PTE_V)) {
			debugf("Invalid request from %08x: no argument page\n", whom);
			continue; // just leave it hanging, waiting for the next request.
		}

		switch (req) {
		case FATREQ_OPEN:
			fat_serve_open(whom, (struct Fatreq_open *)REQVA);
			break;

		case FATREQ_MAP:
			fat_serve_map(whom, (struct Fatreq_map *)REQVA);
			break;

		case FATREQ_SET_SIZE:
			fat_serve_set_size(whom, (struct Fatreq_set_size *)REQVA);
			break;

		case FATREQ_CLOSE:
			fat_serve_close(whom, (struct Fatreq_close *)REQVA);
			break;

		case FATREQ_DIRTY:
			fat_serve_dirty(whom, (struct Fatreq_dirty *)REQVA);
			break;

		case FATREQ_REMOVE:
			fat_serve_remove(whom, (struct Fatreq_remove *)REQVA);
			break;

		case FATREQ_SYNC:
			fat_serve_sync(whom);
			break;
		
		case FATREQ_CREATE:
			fat_serve_create(whom, (struct Fatreq_create *)REQVA);
			break;

		default:
			debugf("Invalid request code %d from %08x\n", whom, req);
			break;
		}

		syscall_mem_unmap(0, (void *)REQVA);
	}
}

int main() {
	user_assert(sizeof(struct FATDIRENT) == BY2DIRENT);

	fat_serve_init();
	fat_fs_init();

	debugf("Init Complete. FAT FS is running.\n");

	serve();
	return 0;
}