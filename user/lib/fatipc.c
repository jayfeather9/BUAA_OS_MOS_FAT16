#include <env.h>
#include <fsreq.h>
#include <lib.h>

#define debug 0

u_char fatipcbuf[BY2PG] __attribute__((aligned(BY2PG)));

// Overview:
//  Send an IPC request to the file server, and wait for a reply.
//
// Parameters:
//  @type: request code, passed as the simple integer IPC value.
//  @fsreq: page to send containing additional request data, usually fatipcbuf.
//          Can be modified by server to return additional response info.
//  @dstva: virtual address at which to receive reply page, 0 if none.
//  @*perm: permissions of received page.
//
// Returns:
//  0 if successful,
//  < 0 on failure.
static int fatipc(u_int type, void *fsreq, void *dstva, u_int *perm) {
	u_int whom;
	// Our fat system server must be the 3nd env.
	ipc_send(envs[2].env_id, type, fsreq, PTE_D);
	return ipc_recv(&whom, dstva, perm);
}

// Overview:
//  Send file-open request to the file server. Includes path and
//  omode in request, sets *fileid and *size from reply.
//
// Returns:
//  0 on success,
//  < 0 on failure.
int fatipc_open(const char *path, u_int omode, struct Fd *fd) {
	u_int perm;
	struct Fsreq_open *req;

	req = (struct Fsreq_open *)fatipcbuf;

	// The path is too long.
	if (strlen(path) >= MAXPATHLEN) {
		return -E_BAD_PATH;
	}

	strcpy((char *)req->req_path, path);
	req->req_omode = omode;
	return fatipc(FSREQ_OPEN, req, fd, &perm);
}

// Overview:
//  Make a map-block request to the file server. We send the fileid and
//  the pageno of the desired part in the file, and the server sends
//  us back a mapping for a page containing that block.
//
// Returns:
//  0 on success,
//  < 0 on failure.
int fatipc_map(u_int fileid, u_int pageno, void *dstva) {
	int r;
	u_int perm;
	struct Fsreq_fatmap *req;

	req = (struct Fsreq_fatmap *)fatipcbuf;
	req->req_fileid = fileid;
	req->req_pageno = pageno;

	if ((r = fatipc(FSREQ_MAP, req, dstva, &perm)) < 0) {
		return r;
	}

	if ((perm & ~(PTE_D | PTE_LIBRARY)) != (PTE_V)) {
		user_panic("fatipc_map: unexpected permissions %08x for dstva %08x", perm, dstva);
	}

	return 0;
}

// Overview:
//  Make a set-file-size request to the file server.
int fatipc_set_size(u_int fileid, u_int size) {
	struct Fsreq_set_size *req;

	req = (struct Fsreq_set_size *)fatipcbuf;
	req->req_fileid = fileid;
	req->req_size = size;
	return fatipc(FSREQ_SET_SIZE, req, 0, 0);
}

// Overview:
//  Make a file-close request to the file server. After this the fileid is invalid.
int fatipc_close(u_int fileid) {
	struct Fsreq_close *req;

	req = (struct Fsreq_close *)fatipcbuf;
	req->req_fileid = fileid;
	return fatipc(FSREQ_CLOSE, req, 0, 0);
}

// Overview:
//  Ask the file server to mark a particular file block dirty.
int fatipc_dirty(u_int fileid, u_int offset) {
	struct Fsreq_dirty *req;

	req = (struct Fsreq_dirty *)fatipcbuf;
	req->req_fileid = fileid;
	req->req_offset = offset;
	return fatipc(FSREQ_DIRTY, req, 0, 0);
}

// Overview:
//  Ask the file server to delete a file, given its path.
int fatipc_remove(const char *path) {
	// Step 1: Check the length of 'path' using 'strlen'.
	// If the length of path is 0 or larger than 'MAXPATHLEN', return -E_BAD_PATH.
	/* Exercise 5.12: Your code here. (1/3) */

	int path_len = strlen(path);
	if (path_len == 0 || path_len > MAXPATHLEN) {
		return -E_BAD_PATH;
	}

	// Step 2: Use 'fatipcbuf' as a 'struct Fsreq_remove'.
	struct Fsreq_remove *req = (struct Fsreq_remove *)fatipcbuf;

	// Step 3: Copy 'path' into the path in 'req' using 'strcpy'.
	/* Exercise 5.12: Your code here. (2/3) */

	strcpy(req->req_path, path);

	// Step 4: Send request to the server using 'fatipc'.
	/* Exercise 5.12: Your code here. (3/3) */

	return fatipc(FSREQ_REMOVE, req, 0, 0);

}

// Overview:
//  Ask the file server to update the disk by writing any dirty
//  blocks in the buffer cache.
int fatipc_sync(void) {
	return fatipc(FSREQ_SYNC, fatipcbuf, 0, 0);
}

int fatipc_fatinit(struct FatBPB *buf) {
	struct Fsreq_fatinit *req = (struct Fsreq_fatinit *)fatipcbuf;
	req->magic = 0xDEADBEEF;
	return fatipc(FSREQ_FATINIT, req, buf, 0);
}