#include <fatfs.h>
#include <lib.h>

#define debug 0

static int fat_file_close(struct Fd *fd);
static int fat_file_read(struct Fd *fd, void *buf, u_int n, u_int offset);
static int fat_file_write(struct Fd *fd, const void *buf, u_int n, u_int offset);
static int fat_file_stat(struct Fd *fd, struct Stat *stat);

// Dot represents choosing the member within the struct declaration
// to initialize, with no need to consider the order of members.
struct Dev devfat = {
    .dev_id = 'a',
    .dev_name = "fat_file",
    .dev_read = fat_file_read,
    .dev_write = fat_file_write,
    .dev_close = fat_file_close,
    .dev_stat = fat_file_stat,
};

u_int clus_size = 0;

void update_clus_size() {
	if (clus_size == 0) {
		clus_size = fatipc_getsize();
		// debugf("updated clus size = %u\n", clus_size);
	}
}

// Overview:
//  Open a file (or directory) in fat disk.
//
// Returns:
//  the file descriptor on success,
//  the underlying error on failure.
int fat_open(const char *path, int mode) {
	update_clus_size();
	// Step 1: Alloc a new 'Fd' using 'fd_alloc' in fd.c.
	struct Fd *fd;
	try(fd_alloc(&fd));

	// Step 2: Prepare the 'fd' using 'fatipc_open' in fatipc.c.
	try(fatipc_open(path, mode, fd));
	
	// Step 3: Set 'va' to the address of the page where the 'fd''s data is cached, using
	// 'fd2data'. Set 'size' and 'fileid' correctly with the value in 'fd' as a 'Filefd'.
	char *va;
	struct Fatfd *ffd;
	u_int size, fileid;

	va = (char *)fd2data(fd);
	ffd = (struct Fatfd *)fd;
	fileid = ffd->f_fileid;
	size = ffd->f_file.DIR_FileSize;

	// Step 4: Alloc pages and map the file content using 'fatipc_map'.
	for (int i = 0; i < size; i += clus_size) {
		void *clus_va = va + (i / clus_size * BY2PG);
		try(fatipc_map(fileid, i, clus_va));
		// debugf("mapped i = %u clus_va = %u\n", i, clus_va);
	}

	// Step 5: Return the number of file descriptor using 'fd2num'.
	return fd2num(fd);
}

// Overview:
//  Close a file descriptor
int fat_file_close(struct Fd *fd) {
	update_clus_size();
	int r;
	struct Fatfd *ffd;
	void *va;
	u_int size, fileid;

	ffd = (struct Fatfd *)fd;
	fileid = ffd->f_fileid;
	size = ffd->f_file.DIR_FileSize;

	// Set the start address storing the file's content.
	va = fd2data(fd);

	// Tell the file server the dirty page.
	for (int i = 0; i < size; i += clus_size) {
		fatipc_dirty(fileid, i);
	}

	// Request the file server to close the file with fatipc.
	if ((r = fatipc_close(fileid)) < 0) {
		debugf("cannot close the file\n");
		return r;
	}

	// Unmap the content of file, release memory.
	if (size == 0) {
		return 0;
	}
	for (int i = 0; i < size; i += clus_size) {
		void *clus_va = va + (i / clus_size * BY2PG);
		if ((r = syscall_mem_unmap(0, clus_va)) < 0) {
			debugf("cannont unmap the file.\n");
			return r;
		}
	}
	return 0;
}

// Overview:
//  Read 'n' bytes from 'fd' at the current seek position into 'buf'. Since files
//  are memory-mapped, this amounts to a memcpy() surrounded by a little red
//  tape to handle the file size and seek pointer.
static int fat_file_read(struct Fd *fd, void *buf, u_int n, u_int offset) {
	update_clus_size();
	u_int size, ori_n;
	struct Fatfd *f;
	f = (struct Fatfd *)fd;

	// Avoid reading past the end of file.
	size = f->f_file.DIR_FileSize;

	if (offset > size) {
		return 0;
	}

	if (offset + n > size) {
		n = size - offset;
	}

	ori_n = n;
	void *va = fd2data(fd);
	for (int i = 0; i < size; i += clus_size) {
		void *clus_va = va + (i / clus_size * BY2PG);
		if (i + clus_size <= offset) {
			continue;
		}
		u_int clus_offset = offset % clus_size;
		u_int read_size = n < clus_size ? n : clus_size;
		memcpy(buf, clus_va + clus_offset, read_size);
		n -= read_size;
		buf += read_size;
		offset = 0;
		if (n == 0) {
			break;
		}
	}
	// memcpy(buf, (char *)fd2data(fd) + offset, n);
	return ori_n;
}

// Overview:
//  Find the virtual address of the page that maps the file block
//  starting at 'offset'.
int fat_read_map(int fdnum, u_int offset, void **blk) {
	update_clus_size();
	int r;
	void *va;
	struct Fd *fd;

	if ((r = fd_lookup(fdnum, &fd)) < 0) {
		return r;
	}

	if (fd->fd_dev_id != devfat.dev_id) {
		return -E_INVAL;
	}

	va = fd2data(fd) + (offset / clus_size * BY2PG) + offset % clus_size;

	if (offset >= MAXFILESIZE) {
		return -E_NO_DISK;
	}

	if (!(vpd[PDX(va)] & PTE_V) || !(vpt[VPN(va)] & PTE_V)) {
		return -E_NO_DISK;
	}

	*blk = (void *)va;
	return 0;
}

// Overview:
//  Write 'n' bytes from 'buf' to 'fd' at the current seek position.
static int fat_file_write(struct Fd *fd, const void *buf, u_int n, u_int offset) {
	update_clus_size();
	int r;
	u_int tot, ori_n;
	struct Fatfd *f;

	f = (struct Fatfd *)fd;

	// Don't write more than the maximum file size.
	tot = offset + n;

	if (tot > MAXFILESIZE) {
		return -E_NO_DISK;
	}

	// Increase the file's size if necessary
	if (tot > f->f_file.DIR_FileSize) {
		if ((r = fat_truncate(fd2num(fd), tot)) < 0) {
			return r;
		}
	}

	ori_n = n;
	// Write the data
	void *va = fd2data(fd);
	for (int i = 0; i < f->f_file.DIR_FileSize; i += clus_size) {
		void *clus_va = va + (i / clus_size * BY2PG);
		if (i + clus_size <= offset) {
			continue;
		}
		u_int clus_offset = offset % clus_size;
		u_int read_size = n < clus_size ? n : clus_size;
		memcpy(clus_va + clus_offset, buf, read_size);
		// debugf("written to va %u for size %u buf[0] = %u\n", clus_va + clus_offset, read_size, *(char *)buf);
		n -= read_size;
		buf += read_size;
		offset = 0;
		if (n == 0) {
			break;
		}
	}
	// memcpy((char *)fd2data(fd) + offset, buf, n);
	return ori_n;
}

static int fat_file_stat(struct Fd *fd, struct Stat *st) {
	update_clus_size();
	struct Fatfd *f;

	f = (struct Fatfd *)fd;

	char name[20];
	int cnt = 0;
	for (int i = 0; i < 11; i++) {
		if (f->f_file.DIR_Name[i] == ' ' || f->f_file.DIR_Name[i] == '\0') continue;
		if (i == 8) name[cnt++] = '.';
		if (i < 8) name[cnt++] = f->f_file.DIR_Name[i];
		else name[cnt++] = f->f_file.DIR_Name[i];
	}
	name[cnt] = '\0';
	strcpy(st->st_name, name);
	st->st_size = f->f_file.DIR_FileSize;
	st->st_isdir = ((f->f_file.DIR_Attr & FAT_ATTR_DIRECTORY) == FAT_ATTR_DIRECTORY);
	return 0;
}

// Overview:
//  Truncate or extend an open file to 'size' bytes
int fat_truncate(int fdnum, u_int size) {
	update_clus_size();
	int i, r;
	struct Fd *fd;
	struct Fatfd *f;
	u_int oldsize, fileid;

	if (size > MAXFILESIZE) {
		return -E_NO_DISK;
	}

	if ((r = fd_lookup(fdnum, &fd)) < 0) {
		return r;
	}

	if (fd->fd_dev_id != devfat.dev_id) {
		return -E_INVAL;
	}

	f = (struct Fatfd *)fd;
	fileid = f->f_fileid;
	oldsize = f->f_file.DIR_FileSize;
	f->f_file.DIR_FileSize = size;

	if ((r = fatipc_set_size(fileid, size)) < 0) {
		return r;
	}

	void *va = fd2data(fd);

	// Map any new pages needed if extending the file
	for (i = ROUND(oldsize, BY2PG); i < ROUND(size, BY2PG); i += BY2PG) {
		if ((r = fatipc_map(fileid, i, va + i)) < 0) {
			fatipc_set_size(fileid, oldsize);
			return r;
		}
	}

	// Unmap pages if truncating the file
	for (i = ROUND(size, BY2PG); i < ROUND(oldsize, BY2PG); i += BY2PG) {
		if ((r = syscall_mem_unmap(0, (void *)(va + i))) < 0) {
			user_panic("fat_truncate: syscall_mem_unmap %08x: %e", va + i, r);
		}
	}

	return 0;
}

// Overview:
//  Delete a file or directory.
int fat_remove(const char *path) {
	update_clus_size();
	return fatipc_remove(path);

}

// Overview:
//  Synchronize disk with buffer cache
int fat_sync(void) {
	update_clus_size();
	return fatipc_sync();
}

int fat_create(const char *path, u_int attr, u_int size) {
	update_clus_size();
	return fatipc_create(path, attr, size);
}