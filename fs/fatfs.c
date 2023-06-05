#include "fatserv.h"
#include <mmu.h>

struct FatSuper *super;

struct FATBPB fatBPB;
struct FATDISK fatDisk;
unsigned char zero_buffer[FAT_MAX_CLUS_SIZE];

// =============================================================================
//
// below is fat space managing part

struct FatSpace fat_spaces[FAT_MAX_SPACE_SIZE];
struct FatSpace fat_space_head, fat_space_tail;

struct FatSpace *alloc_fat_space(uint32_t st_va, uint32_t size, uint32_t clus) {
	for (int i = 0; i < FAT_MAX_SPACE_SIZE; i++) {
		if (fat_spaces[i].st_va == 0) {
			fat_spaces[i].st_va = st_va;
			fat_spaces[i].size = size;
			fat_spaces[i].clus = clus;
			return &fat_spaces[i];
		}
	}
	return NULL;
}

void insert_head_fat_space_list(uint32_t st_va, uint32_t size, uint32_t clus) {
	struct FatSpace *fsp = alloc_fat_space(st_va, size, clus);
	fsp->prev = &fat_space_head;
	fsp->nxt = fat_space_head.nxt;
	fat_space_head.nxt->prev = fsp;
	fat_space_head.nxt = fsp;
	return;
}

void remove_fat_space_list(struct FatSpace *fsp) {
	fsp->prev->nxt = fsp->nxt;
	fsp->nxt->prev = fsp->prev;
	return;
}

int insert_space(uint32_t st_va, uint32_t size) {
	debugf("inserting space [0x%X, 0x%X]\n", st_va, st_va + size);
	int r;
	struct FatSpace *fspace;
	for (fspace = fat_space_head.nxt; fspace != (&fat_space_tail); fspace = fspace->nxt) {
		if (fspace->st_va + fspace->size == st_va && fspace->clus == 0) {
			remove_fat_space_list(fspace);
			r = insert_space(fspace->st_va, fspace->size + size);
			fspace->st_va = 0;
			return r;
		}
	}
	for (fspace = fat_space_head.nxt; fspace != (&fat_space_tail); fspace = fspace->nxt) {
		if (fspace->st_va == st_va + size && fspace->clus == 0) {
			remove_fat_space_list(fspace);
			r = insert_space(st_va, fspace->size + size);
			fspace->st_va = 0;
			return r;
		}
	}
	insert_head_fat_space_list(st_va, size, 0);
	return 0;
}

// Overview:
//  Check if this cluster is mapped in cache.
//  if va set, va will be set to the va of the stating va for the cluster
int is_clus_mapped(uint32_t clus, uint32_t *va) {
	struct FatSpace *fspace;
	for (fspace = fat_space_head.nxt; fspace != (&fat_space_tail); fspace = fspace->nxt) {
		if (fspace->clus == clus) {
			if (va) {
				*va = fspace->st_va;
			}
			return 1;
		}
	}
	return 0;
}

// Overview:
// free the cluster space in cache
int free_clus(uint32_t clus) {
	int r;
	struct FatSpace *fspace;
	for (fspace = fat_space_head.nxt; fspace != (&fat_space_tail); fspace = fspace->nxt) {
		if (fspace->clus == clus) {
			remove_fat_space_list(fspace);
			r = insert_space(fspace->st_va, fspace->size);
			fspace->st_va = 0;
			return r;
		}
	}
	return -E_FAT_NOT_FOUND;
}

void debug_print_fspace() {
	struct FatSpace *fspace;
	for (fspace = fat_space_head.nxt; fspace != (&fat_space_tail); fspace = fspace->nxt) {
		debugf("[0x%X, 0x%X] clus = %d\n", fspace->st_va, fspace->st_va + fspace->size, fspace->clus);
	}
}

// Overview:
// alloc a space in cache for the cluster
int alloc_fat_file_space(uint32_t clus, uint32_t bysize, uint32_t *va) {
	bysize = ROUND(bysize, BY2PG);
	if (is_clus_mapped(clus, va)) {
		return 0;
	}
	struct FatSpace *fspace;
	for (fspace = fat_space_head.nxt; fspace != (&fat_space_tail); fspace = fspace->nxt) {
		if (fspace->clus == 0 && fspace->size >= bysize) {
			if (va) {
				*va = fspace->st_va;
			}
			remove_fat_space_list(fspace);
			insert_head_fat_space_list(fspace->st_va, bysize, clus);
			if (bysize < fspace->size) {
				insert_space(fspace->st_va + bysize, fspace->size - bysize); 
			}
			fspace->st_va = 0;
			return 0;
		}
	}
	return -E_FAT_VA_FULL;
}
// end of fat space managing part
// =============================================================================


// =============================================================================
//
// below is fat cluster managing part

int is_bad_cluster(uint32_t clus) {
	return (clus >= fatDisk.CountofClusters) || (clus < 2);
}

int read_fat_cluster(uint32_t clus, unsigned char *buf) {
	if (clus == 0) {
		ide_read(DISKNO, fatDisk.FirstRootDirSecNum, buf, fatDisk.RootDirSectors);
		return 0;
	}
	if (is_bad_cluster(clus)) {
		return -E_FAT_BAD_CLUSTER;
	}
	uint32_t fat_sec = fatBPB.RsvdSecCnt + fatBPB.NumFATs * fatDisk.FATSz + fatDisk.RootDirSectors + (clus - 2) * fatBPB.SecPerClus;
	ide_read(DISKNO, fat_sec, tmp_buf, fatBPB.SecPerClus);
	nbyts = nbyts > fatBPB.SecPerClus * fatBPB.BytsPerSec ? fatBPB.SecPerClus * fatBPB.BytsPerSec : nbyts;
	for (int i = 0; i < nbyts; i++) buf[i] = tmp_buf[i];
	return 0;
}

int write_fat_cluster(uint32_t clus, unsigned char *buf, uint32_t nbyts) {
	unsigned char tmp_buf[FAT_MAX_FILE_SIZE];
	if (clus == 0) {
		nbyts = nbyts > fatDisk.RootDirSectors * fatBPB.BytsPerSec ? fatDisk.RootDirSectors * fatBPB.BytsPerSec : nbyts;
		ide_read(DISKNO, fatDisk.FirstRootDirSecNum, tmp_buf, fatDisk.RootDirSectors);
		for (int i = 0; i < nbyts; i++) tmp_buf[i] = buf[i];
		ide_write(DISKNO, fatDisk.FirstRootDirSecNum, tmp_buf, fatDisk.RootDirSectors);
		return 0;
	}
	if (is_bad_cluster(clus)) {
		return -E_FAT_BAD_CLUSTER;
	}
	if (is_free_cluster(clus)) {
		return -E_FAT_ACCESS_FREE_CLUSTER;
	}
	if (nbyts > fatBPB.SecPerClus * fatBPB.BytsPerSec) {
		nbyts = fatBPB.SecPerClus * fatBPB.BytsPerSec;
	}
	uint32_t fat_sec = fatBPB.RsvdSecCnt + fatBPB.NumFATs * fatDisk.FATSz + fatDisk.RootDirSectors + (clus - 2) * fatBPB.SecPerClus;
	ide_read(DISKNO, fat_sec, tmp_buf, fatBPB.SecPerClus);
	nbyts = nbyts > fatBPB.SecPerClus * fatBPB.BytsPerSec ? fatBPB.SecPerClus * fatBPB.BytsPerSec : nbyts;
	for (int i = 0; i < nbyts; i++) tmp_buf[i] = buf[i];
	ide_write(DISKNO, fat_sec, tmp_buf, fatBPB.SecPerClus);
	return 0;
}

int read_fat_clusters(uint32_t clus, unsigned char *buf, uint32_t nbyts) {
	if (clus == 0) {
		return read_fat_cluster(0, buf, nbyts);
	}
	uint32_t prev_clus, entry_val = 0x0, finished_byts = 0;
	for (prev_clus = clus; entry_val != 0xFFFF; prev_clus = entry_val) {
		try(get_fat_entry(prev_clus, &entry_val));
		//try(read_fat_cluster(prev_clus, buf));
		finished_byts += fatBPB.BytsPerSec * fatBPB.SecPerClus;
		if (finished_byts < nbyts)
			try(read_fat_cluster(prev_clus, buf, fatBPB.BytsPerSec * fatBPB.SecPerClus));
		else
			try(read_fat_cluster(prev_clus, buf, fatBPB.BytsPerSec * fatBPB.SecPerClus - (finished_byts - nbyts)));
		if (finished_byts >= nbyts)break;
		buf += fatBPB.BytsPerSec * fatBPB.SecPerClus;
	}
	return 0;
}

int write_fat_clusters(uint32_t clus, unsigned char *buf, uint32_t nbyts) {
	if (clus == 0) {
		return write_fat_cluster(0, buf, nbyts);
	}
	uint32_t prev_clus, entry_val = 0x0, finished_byts = 0;
	for (prev_clus = clus; entry_val != 0xFFFF; prev_clus = entry_val) {
		try(get_fat_entry(prev_clus, &entry_val));
		finished_byts += fatBPB.BytsPerSec * fatBPB.SecPerClus;
		if (finished_byts < nbyts)
			try(write_fat_cluster(prev_clus, buf, fatBPB.BytsPerSec * fatBPB.SecPerClus));
		else
			try(write_fat_cluster(prev_clus, buf, fatBPB.BytsPerSec * fatBPB.SecPerClus - (finished_byts - nbyts)));
		if (finished_byts >= nbyts)break;
		buf += fatBPB.BytsPerSec * fatBPB.SecPerClus;
	}
	return 0;
}

void debug_print_cluster_data(uint32_t clus) {
	unsigned char buf[16384];
	read_fat_cluster(clus, buf, FAT_MAX_CLUS_SIZE);
	debugf("========= printing cluster %u =========\n", clus);
	for (int i = 0; i < 32; i++) {
		debugf("0x%4x-0x%4x: ", i*16, i*16+15);
		for (int j = 0; j < 16; j++) {
			debugf("%02X ", buf[i*16+j]);
		}
		debugf("\n");
	}
	debugf("========= end of cluster %u ===========\n", clus);
}
// end of fat cluster managing part
// =============================================================================

void fat_file_flush(struct File *);
int fat_block_is_free(u_int);

// Overview:
//  Return the virtual address of this disk block in cache.
// UNUSABLE
void *fat_diskaddr(u_int blockno) {
	user_panic("usage of unusable function!\n");
}

// Overview:
//  Check if this virtual address is mapped to a block. (check PTE_V bit)
int fat_va_is_mapped(void *va) {
	return (vpd[PDX(va)] & PTE_V) && (vpt[VPN(va)] & PTE_V);
}

// Overview:
//  Check if this virtual address is dirty. (check PTE_DIRTY bit)
int fat_va_is_dirty(void *va) {
	return vpt[VPN(va)] & PTE_DIRTY;
}

// Overview:
//  Check if this cluster is dirty. (check corresponding `va`)
int fat_clus_is_dirty(u_int clus) {
	void *va;
	if(is_clus_mapped(clus, va)){
		return fat_va_is_mapped(va) && fat_va_is_dirty(va);
	}
	return 0;
}

// Overview:
//  Mark this cluster as dirty (cache page has changed and needs to be written back to disk).
int fat_dirty_clus(u_int clus) {
	void *va;
	if (!is_clus_mapped(clus, va)) {
		return -E_FAT_CLUS_UNMAPPED;
	}

	if (!fat_va_is_mapped(va)) {
		return -E_FAT_NOT_FOUND;
	}

	if (fat_va_is_dirty(va)) {
		return 0;
	}

	return syscall_mem_map(0, va, 0, va, PTE_D | PTE_DIRTY);
}

// Overview:
//  Write the current contents of the cluster out to disk.
void fat_write_clus(u_int clus) {
	void *va;
	// Step 1: detect is this block is mapped, if not, can't write it's data to disk.
	if (!is_clus_mapped(clus, va)) {
		user_panic("write unmapped cluster %d", clus);
	}

	// Step2: write data to IDE disk. (using ide_write, and the diskno is 0)
	ide_write(0, blockno * SECT2BLK, va, SECT2BLK);
}

// Overview:
//  Make sure a particular disk block is loaded into memory.
//
// Post-Condition:
//  Return 0 on success, or a negative error code on error.
//
//  If blk!=0, set *blk to the address of the block in memory.
//
//  If isnew!=0, set *isnew to 0 if the block was already in memory, or
//  to 1 if the block was loaded off disk to satisfy this request. (Isnew
//  lets callers like file_get_block clear any memory-only fields
//  from the disk blocks when they come in off disk.)
//
// Hint:
//  use diskaddr, block_is_mapped, syscall_mem_alloc, and ide_read.
int fat_read_block(u_int blockno, void **blk, u_int *isnew) {
	// Step 1: validate blockno. Make file the block to read is within the disk.
	if (super && blockno >= super->s_nblocks) {
		user_panic("reading non-existent block %08x\n", blockno);
	}

	// Step 2: validate this block is used, not free.
	// Hint:
	//  If the bitmap is NULL, indicate that we haven't read bitmap from disk to memory
	//  until now. So, before we check if a block is free using `block_is_free`, we must
	//  ensure that the bitmap blocks are already read from the disk to memory.
	if (bitmap && fat_block_is_free(blockno)) {
		user_panic("reading free block %08x\n", blockno);
	}

	// Step 3: transform block number to corresponding virtual address.
	void *va = fat_diskaddr(blockno);

	// Step 4: read disk and set *isnew.
	// Hint:
	//  If this block is already mapped, just set *isnew, else alloc memory and
	//  read data from IDE disk (use `syscall_mem_alloc` and `ide_read`).
	//  We have only one IDE disk, so the diskno of ide_read should be 0.
	if (fat_block_is_mapped(blockno)) { // the block is in memory
		if (isnew) {
			*isnew = 0;
		}
	} else { // the block is not in memory
		if (isnew) {
			*isnew = 1;
		}
		syscall_mem_alloc(0, va, PTE_D);
		ide_read(0, blockno * SECT2BLK, va, SECT2BLK);
	}

	// Step 5: if blk != NULL, assign 'va' to '*blk'.
	if (blk) {
		*blk = va;
	}
	return 0;
}

// Overview:
//  Allocate a page to cache the disk block.
int fat_map_block(u_int blockno) {
	// Step 1: If the block is already mapped in cache, return 0.
	// Hint: Use 'block_is_mapped'.
	/* Exercise 5.7: Your code here. (1/5) */

	if (fat_block_is_mapped(blockno)) {
		return 0;
	}

	// Step 2: Alloc a page in permission 'PTE_D' via syscall.
	// Hint: Use 'diskaddr' for the virtual address.
	/* Exercise 5.7: Your code here. (2/5) */

	void *va = fat_diskaddr(blockno);
	return syscall_mem_alloc(0, va, PTE_D);
}

// Overview:
//  Unmap a disk block in cache.
void fat_unmap_block(u_int blockno) {
	// Step 1: Get the mapped address of the cache page of this block using 'block_is_mapped'.
	void *va;
	/* Exercise 5.7: Your code here. (3/5) */

	va = fat_block_is_mapped(blockno);
	user_assert(va);

	// Step 2: If this block is used (not free) and dirty in cache, write it back to the disk
	// first.
	// Hint: Use 'block_is_free', 'block_is_dirty' to check, and 'write_block' to sync.
	/* Exercise 5.7: Your code here. (4/5) */

	if (!fat_block_is_free(blockno) && fat_block_is_dirty(blockno)) {
		fat_write_block(blockno);
	}

	// Step 3: Unmap the virtual address via syscall.
	/* Exercise 5.7: Your code here. (5/5) */
	user_assert(!syscall_mem_unmap(0, va));

	user_assert(!fat_block_is_mapped(blockno));
}

// Overview:
//  Check if the block 'blockno' is free via bitmap.
//
// Post-Condition:
//  Return 1 if the block is free, else 0.
int fat_block_is_free(u_int blockno) {
	if (super == 0 || blockno >= super->s_nblocks) {
		return 0;
	}

	if (bitmap[blockno / 32] & (1 << (blockno % 32))) {
		return 1;
	}

	return 0;
}

// Overview:
//  Mark a block as free in the bitmap.
void fat_free_block(u_int blockno) {
	// You can refer to the function 'block_is_free' above.
	// Step 1: If 'blockno' is invalid (0 or >= the number of blocks in 'super'), return.
	/* Exercise 5.4: Your code here. (1/2) */
	if (blockno == 0 || (super && blockno >= super->s_nblocks)) {
		return;
	}

	// Step 2: Set the flag bit of 'blockno' in 'bitmap'.
	// Hint: Use bit operations to update the bitmap, such as b[n / W] |= 1 << (n % W).
	/* Exercise 5.4: Your code here. (2/2) */
	bitmap[blockno / 32] |= (1 << (blockno % 32));

}

// Overview:
//  Search in the bitmap for a free block and allocate it.
//
// Post-Condition:
//  Return block number allocated on success,
//  Return -E_NO_DISK if we are out of blocks.
int fat_alloc_block_num(void) {
	int blockno;
	// walk through this bitmap, find a free one and mark it as used, then sync
	// this block to IDE disk (using `write_block`) from memory.
	for (blockno = 3; blockno < super->s_nblocks; blockno++) {
		if (bitmap[blockno / 32] & (1 << (blockno % 32))) { // the block is free
			bitmap[blockno / 32] &= ~(1 << (blockno % 32));
			fat_write_block(blockno / BIT2BLK + 2); // write to disk.
			return blockno;
		}
	}
	// no free blocks.
	return -E_NO_DISK;
}

// Overview:
//  Allocate a block -- first find a free block in the bitmap, then map it into memory.
int fat_alloc_block(void) {
	int r, bno;
	// Step 1: find a free block.
	if ((r = fat_alloc_block_num()) < 0) { // failed.
		return r;
	}
	bno = r;

	// Step 2: map this block into memory.
	if ((r = fat_map_block(bno)) < 0) {
		fat_free_block(bno);
		return r;
	}

	// Step 3: return block number.
	return bno;
}

// Overview:
//  Read and validate the file system super-block.
//
// Post-condition:
//  If error occurred during read super block or validate failed, panic.
void fat_read_super(void) {
	int r;
	void *blk;

	// Step 1: read super block.
	if ((r = fat_read_block(1, &blk, 0)) < 0) {
		user_panic("cannot read superblock: %e", r);
	}

	super = blk;

	// Step 2: Check fs magic nunber.
	if (super->s_magic != FS_MAGIC) {
		user_panic("bad file system magic number %x %x", super->s_magic, FS_MAGIC);
	}

	// Step 3: validate disk size.
	if (super->s_nblocks > DISKMAX / BY2BLK) {
		user_panic("file system is too large");
	}

	debugf("superblock is good\n");
}

// Overview:
//  Read and validate the file system bitmap.
//
// Hint:
//  Read all the bitmap blocks into memory.
//  Set the 'bitmap' to point to the first bitmap block.
//  For each block i, user_assert(!fat_block_is_free(i))) to check that they're all marked as in use.
void fat_read_bitmap(void) {
	u_int i;
	void *blk = NULL;

	// Step 1: Calculate the number of the bitmap blocks, and read them into memory.
	u_int nbitmap = super->s_nblocks / BIT2BLK + 1;
	for (i = 0; i < nbitmap; i++) {
		fat_read_block(i + 2, blk, 0);
	}

	bitmap = fat_diskaddr(2);

	// Step 2: Make sure the reserved and root blocks are marked in-use.
	// Hint: use `block_is_free`
	user_assert(!fat_block_is_free(0));
	user_assert(!fat_block_is_free(1));

	// Step 3: Make sure all bitmap blocks are marked in-use.
	for (i = 0; i < nbitmap; i++) {
		user_assert(!fat_block_is_free(i + 2));
	}

	debugf("read_bitmap is good\n");
}

// Overview:
//  Test that write_block works, by smashing the superblock and reading it back.
void fat_check_write_block(void) {
	super = 0;

	// backup the super block.
	// copy the data in super block to the first block on the disk.
	fat_read_block(0, 0, 0);
	memcpy((char *)fat_diskaddr(0), (char *)fat_diskaddr(1), BY2PG);

	// smash it
	strcpy((char *)fat_diskaddr(1), "OOPS!\n");
	fat_write_block(1);
	user_assert(fat_block_is_mapped(1));

	// clear it out
	syscall_mem_unmap(0, fat_diskaddr(1));
	user_assert(!fat_block_is_mapped(1));

	// validate the data read from the disk.
	fat_read_block(1, 0, 0);
	user_assert(strcmp((char *)fat_diskaddr(1), "OOPS!\n") == 0);

	// restore the super block.
	memcpy((char *)fat_diskaddr(1), (char *)fat_diskaddr(0), BY2PG);
	fat_write_block(1);
	super = (struct Super *)fat_diskaddr(1);
}

// Overview:
//  Initialize the file system.
// Hint:
//  1. read super block.
//  2. check if the disk can work.
//  3. read bitmap blocks from disk to memory.
void fat_fs_init(void) {
	fat_read_super();
	fat_check_write_block();
	fat_read_bitmap();
}

// Overview:
//  Like pgdir_walk but for files.
//  Find the disk block number slot for the 'filebno'th block in file 'f'. Then, set
//  '*ppdiskbno' to point to that slot. The slot will be one of the f->f_direct[] entries,
//  or an entry in the indirect block.
//  When 'alloc' is set, this function will allocate an indirect block if necessary.
//
// Post-Condition:
//  Return 0 on success, and set *ppdiskbno to the pointer to the target block.
//  Return -E_NOT_FOUND if the function needed to allocate an indirect block, but alloc was 0.
//  Return -E_NO_DISK if there's no space on the disk for an indirect block.
//  Return -E_NO_MEM if there's not enough memory for an indirect block.
//  Return -E_INVAL if filebno is out of range (>= NINDIRECT).
int fat_file_block_walk(struct File *f, u_int filebno, uint32_t **ppdiskbno, u_int alloc) {
	int r;
	uint32_t *ptr;
	uint32_t *blk;

	if (filebno < NDIRECT) {
		// Step 1: if the target block is corresponded to a direct pointer, just return the
		// disk block number.
		ptr = &f->f_direct[filebno];
	} else if (filebno < NINDIRECT) {
		// Step 2: if the target block is corresponded to the indirect block, but there's no
		//  indirect block and `alloc` is set, create the indirect block.
		if (f->f_indirect == 0) {
			if (alloc == 0) {
				return -E_NOT_FOUND;
			}

			if ((r = fat_alloc_block()) < 0) {
				return r;
			}
			f->f_indirect = r;
		}

		// Step 3: read the new indirect block to memory.
		if ((r = fat_read_block(f->f_indirect, (void **)&blk, 0)) < 0) {
			return r;
		}
		ptr = blk + filebno;
	} else {
		return -E_INVAL;
	}

	// Step 4: store the result into *ppdiskbno, and return 0.
	*ppdiskbno = ptr;
	return 0;
}

// OVerview:
//  Set *diskbno to the disk block number for the filebno'th block in file f.
//  If alloc is set and the block does not exist, allocate it.
//
// Post-Condition:
//  Returns 0: success, < 0 on error.
//  Errors are:
//   -E_NOT_FOUND: alloc was 0 but the block did not exist.
//   -E_NO_DISK: if a block needed to be allocated but the disk is full.
//   -E_NO_MEM: if we're out of memory.
//   -E_INVAL: if filebno is out of range.
int fat_file_map_block(struct File *f, u_int filebno, u_int *diskbno, u_int alloc) {
	int r;
	uint32_t *ptr;

	// Step 1: find the pointer for the target block.
	if ((r = fat_file_block_walk(f, filebno, &ptr, alloc)) < 0) {
		return r;
	}

	// Step 2: if the block not exists, and create is set, alloc one.
	if (*ptr == 0) {
		if (alloc == 0) {
			return -E_NOT_FOUND;
		}

		if ((r = fat_alloc_block()) < 0) {
			return r;
		}
		*ptr = r;
	}

	// Step 3: set the pointer to the block in *diskbno and return 0.
	*diskbno = *ptr;
	return 0;
}

// Overview:
//  Remove a block from file f. If it's not there, just silently succeed.
int fat_file_clear_block(struct File *f, u_int filebno) {
	int r;
	uint32_t *ptr;

	if ((r = fat_file_block_walk(f, filebno, &ptr, 0)) < 0) {
		return r;
	}

	if (*ptr) {
		fat_free_block(*ptr);
		*ptr = 0;
	}

	return 0;
}

// Overview:
//  Set *blk to point at the filebno'th block in file f.
//
// Hint: use file_map_block and read_block.
//
// Post-Condition:
//  return 0 on success, and read the data to `blk`, return <0 on error.
int fat_file_get_block(struct File *f, u_int filebno, void **blk) {
	int r;
	u_int diskbno;
	u_int isnew;

	// Step 1: find the disk block number is `f` using `file_map_block`.
	if ((r = fat_file_map_block(f, filebno, &diskbno, 1)) < 0) {
		return r;
	}

	// Step 2: read the data in this disk to blk.
	if ((r = fat_read_block(diskbno, blk, &isnew)) < 0) {
		return r;
	}
	return 0;
}

// Overview:
//  Mark the offset/BY2BLK'th block dirty in file f.
int fat_file_dirty(struct File *f, u_int offset) {
	int r;
	u_int diskbno;

	if ((r = fat_file_map_block(f, offset / BY2BLK, &diskbno, 0)) < 0) {
		return r;
	}

	return fat_dirty_block(diskbno);
}

// Overview:
//  Find a file named 'name' in the directory 'dir'. If found, set *file to it.
//
// Post-Condition:
//  Return 0 on success, and set the pointer to the target file in `*file`.
//  Return the underlying error if an error occurs.
int fat_dir_lookup(struct File *dir, char *name, struct File **file) {
	// int r;
	// Step 1: Calculate the number of blocks in 'dir' via its size.
	u_int nblock;
	/* Exercise 5.8: Your code here. (1/3) */

	nblock = dir->f_size / BY2BLK;

	// Step 2: Iterate through all blocks in the directory.
	for (int i = 0; i < nblock; i++) {
		// Read the i'th block of 'dir' and get its address in 'blk' using 'file_get_block'.
		void *blk;
		/* Exercise 5.8: Your code here. (2/3) */

		try(fat_file_get_block(dir, i, &blk));

		struct File *files = (struct File *)blk;

		// Find the target among all 'File's in this block.
		for (struct File *f = files; f < files + FILE2BLK; ++f) {
			// Compare the file name against 'name' using 'strcmp'.
			// If we find the target file, set '*file' to it and set up its 'f_dir'
			// field.
			/* Exercise 5.8: Your code here. (3/3) */

			if (strcmp(f->f_name, name) == 0) {
				*file = f;
				f->f_dir = dir;
				return 0;
			}
		}
	}

	return -E_NOT_FOUND;
}

// Overview:
//  Alloc a new File structure under specified directory. Set *file
//  to point at a free File structure in dir.
int fat_dir_alloc_file(struct File *dir, struct File **file) {
	int r;
	u_int nblock, i, j;
	void *blk;
	struct File *f;

	nblock = dir->f_size / BY2BLK;

	for (i = 0; i < nblock; i++) {
		// read the block.
		if ((r = fat_file_get_block(dir, i, &blk)) < 0) {
			return r;
		}

		f = blk;

		for (j = 0; j < FILE2BLK; j++) {
			if (f[j].f_name[0] == '\0') { // found free File structure.
				*file = &f[j];
				return 0;
			}
		}
	}

	// no free File structure in exists data block.
	// new data block need to be created.
	dir->f_size += BY2BLK;
	if ((r = fat_file_get_block(dir, i, &blk)) < 0) {
		return r;
	}
	f = blk;
	*file = &f[0];

	return 0;
}

// Overview:
//  Skip over slashes.
char *fat_skip_slash(char *p) {
	while (*p == '/') {
		p++;
	}
	return p;
}

// Overview:
//  Evaluate a path name, starting at the root.
//
// Post-Condition:
//  On success, set *pfile to the file we found and set *pdir to the directory
//  the file is in.
//  If we cannot find the file but find the directory it should be in, set
//  *pdir and copy the final path element into lastelem.
int fat_walk_path(char *path, struct File **pdir, struct File **pfile, char *lastelem) {
	char *p;
	char name[MAXNAMELEN];
	struct File *dir, *file;
	int r;

	// start at the root.
	path = fat_skip_slash(path);
	file = &super->s_root;
	dir = 0;
	name[0] = 0;

	if (pdir) {
		*pdir = 0;
	}

	*pfile = 0;

	// find the target file by name recursively.
	while (*path != '\0') {
		dir = file;
		p = path;

		while (*path != '/' && *path != '\0') {
			path++;
		}

		if (path - p >= MAXNAMELEN) {
			return -E_BAD_PATH;
		}

		memcpy(name, p, path - p);
		name[path - p] = '\0';
		path = fat_skip_slash(path);
		if (dir->f_type != FTYPE_DIR) {
			return -E_NOT_FOUND;
		}

		if ((r = fat_dir_lookup(dir, name, &file)) < 0) {
			if (r == -E_NOT_FOUND && *path == '\0') {
				if (pdir) {
					*pdir = dir;
				}

				if (lastelem) {
					strcpy(lastelem, name);
				}

				*pfile = 0;
			}

			return r;
		}
	}

	if (pdir) {
		*pdir = dir;
	}

	*pfile = file;
	return 0;
}

// Overview:
//  Open "path".
//
// Post-Condition:
//  On success set *pfile to point at the file and return 0.
//  On error return < 0.
int fat_file_open(char *path, struct File **file) {
	return fat_walk_path(path, 0, file, 0);
}

// Overview:
//  Create "path".
//
// Post-Condition:
//  On success set *file to point at the file and return 0.
//  On error return < 0.
int fat_file_create(char *path, struct File **file) {
	char name[MAXNAMELEN];
	int r;
	struct File *dir, *f;

	if ((r = fat_walk_path(path, &dir, &f, name)) == 0) {
		return -E_FILE_EXISTS;
	}

	if (r != -E_NOT_FOUND || dir == 0) {
		return r;
	}

	if (fat_dir_alloc_file(dir, &f) < 0) {
		return r;
	}

	strcpy(f->f_name, name);
	*file = f;
	return 0;
}

// Overview:
//  Truncate file down to newsize bytes.
//
//  Since the file is shorter, we can free the blocks that were used by the old
//  bigger version but not by our new smaller self. For both the old and new sizes,
//  figure out the number of blocks required, and then clear the blocks from
//  new_nblocks to old_nblocks.
//
//  If the new_nblocks is no more than NDIRECT, free the indirect block too.
//  (Remember to clear the f->f_indirect pointer so you'll know whether it's valid!)
//
// Hint: use file_clear_block.
void fat_file_truncate(struct File *f, u_int newsize) {
	u_int bno, old_nblocks, new_nblocks;

	old_nblocks = f->f_size / BY2BLK + 1;
	new_nblocks = newsize / BY2BLK + 1;

	if (newsize == 0) {
		new_nblocks = 0;
	}

	if (new_nblocks <= NDIRECT) {
		for (bno = new_nblocks; bno < old_nblocks; bno++) {
			fat_file_clear_block(f, bno);
		}
		if (f->f_indirect) {
			fat_free_block(f->f_indirect);
			f->f_indirect = 0;
		}
	} else {
		for (bno = new_nblocks; bno < old_nblocks; bno++) {
			fat_file_clear_block(f, bno);
		}
	}

	f->f_size = newsize;
}

// Overview:
//  Set file size to newsize.
int fat_file_set_size(struct File *f, u_int newsize) {
	if (f->f_size > newsize) {
		fat_file_truncate(f, newsize);
	}

	f->f_size = newsize;

	if (f->f_dir) {
		fat_file_flush(f->f_dir);
	}

	return 0;
}

// Overview:
//  Flush the contents of file f out to disk.
//  Loop over all the blocks in file.
//  Translate the file block number into a disk block number and then
//  check whether that disk block is dirty. If so, write it out.
//
// Hint: use file_map_block, block_is_dirty, and write_block.
void fat_file_flush(struct File *f) {
	// Your code here
	u_int nblocks;
	u_int bno;
	u_int diskno;
	int r;

	nblocks = f->f_size / BY2BLK + 1;

	for (bno = 0; bno < nblocks; bno++) {
		if ((r = fat_file_map_block(f, bno, &diskno, 0)) < 0) {
			continue;
		}
		if (fat_block_is_dirty(diskno)) {
			fat_write_block(diskno);
		}
	}
}

// Overview:
//  Sync the entire file system.  A big hammer.
void fat_fs_sync(void) {
	int i;
	for (i = 0; i < super->s_nblocks; i++) {
		if (fat_block_is_dirty(i)) {
			fat_write_block(i);
		}
	}
}

// Overview:
//  Close a file.
void fat_file_close(struct File *f) {
	// Flush the file itself, if f's f_dir is set, flush it's f_dir.
	fat_file_flush(f);
	if (f->f_dir) {
		fat_file_flush(f->f_dir);
	}
}

// Overview:
//  Remove a file by truncating it and then zeroing the name.
int fat_file_remove(char *path) {
	int r;
	struct File *f;

	// Step 1: find the file on the disk.
	if ((r = fat_walk_path(path, 0, &f, 0)) < 0) {
		return r;
	}

	// Step 2: truncate it's size to zero.
	fat_file_truncate(f, 0);

	// Step 3: clear it's name.
	f->f_name[0] = '\0';

	// Step 4: flush the file.
	fat_file_flush(f);
	if (f->f_dir) {
		fat_file_flush(f->f_dir);
	}

	return 0;
}
