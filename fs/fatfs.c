#include "fatserv.h"
#include <mmu.h>

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
	if (is_clus_mapped(clus, &va)) {
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
// below is fat entry managing part

int is_free_cluster(uint32_t clus) {
	uint32_t entry_val;
	get_fat_entry(clus, &entry_val);
	return (entry_val == 0);
}

int is_different_buffers(uint32_t n, unsigned char *fat_buf1, unsigned char *fat_buf2) {
	for (int i = 0; i < n; i++) {
		if (fat_buf1[i] != fat_buf2[i]) {
			return 1;
		}
	}
	return 0;
}

int get_fat_entry(uint32_t clus, uint32_t *pentry_val) {
	if (is_bad_cluster(clus)) {
		return -E_FAT_BAD_CLUSTER;
	}
	uint32_t fat_offset = clus * 2;
	uint32_t fat_sec = fatBPB.BPB_RsvdSecCnt + (fat_offset / fatBPB.BPB_BytsPerSec);
	uint32_t fat_ent_offset = (fat_offset % fatBPB.BPB_BytsPerSec);

	unsigned char fat_buf1[FAT_MAX_CLUS_SIZE], fat_buf2[FAT_MAX_CLUS_SIZE];
	ide_read(DISKNO, fat_sec, fat_buf1, 1);

	// check all FATs same
	for (int i = 1; i < fatBPB.BPB_NumFATs; i++) {
		uint32_t other_fat_sec = (i * fatDisk.FATSz) + fat_sec;
		ide_read(DISKNO, other_fat_sec, fat_buf2, 1);
		if (is_different_buffers(fatBPB.BPB_BytsPerSec, fat_buf1, fat_buf2)) {
			return -E_FAT_ENT_DIFF;
		}
	}

	unsigned char *tmp_buf = fat_buf1 + fat_ent_offset;
	// debugf("reading buf = %02X %02X, offset = %u\n", tmp_buf[0], tmp_buf[1], fat_ent_offset);
	read_little_endian(&tmp_buf, 2, pentry_val);
	return 0;
}

int set_fat_entry(uint32_t clus, uint32_t entry_val) {
	if (is_bad_cluster(clus)) {
		return -E_FAT_BAD_CLUSTER;
	}
	uint32_t fat_offset = clus * 2;
	uint32_t fat_sec = fatBPB.BPB_RsvdSecCnt + fat_offset / fatBPB.BPB_BytsPerSec;
	uint32_t fat_ent_offset = fat_offset % fatBPB.BPB_BytsPerSec;
	unsigned char fat_buf1[FAT_MAX_CLUS_SIZE], fat_buf2[FAT_MAX_CLUS_SIZE];
	ide_read(DISKNO, fat_sec, fat_buf1, 1);

	// check all FATs same
	for (int i = 1; i < fatBPB.BPB_NumFATs; i++) {
		uint32_t other_fat_sec = (i * fatDisk.FATSz) + fat_sec;
		ide_read(DISKNO, other_fat_sec, fat_buf2, 1);
		if (is_different_buffers(fatBPB.BPB_BytsPerSec, fat_buf1, fat_buf2)) {
			return -E_FAT_ENT_DIFF;
		}
	}

	unsigned char *tmp_buf = fat_buf1 + fat_ent_offset;
	write_little_endian(&tmp_buf, 2, entry_val);
	ide_write(DISKNO, fat_sec, fat_buf1, 1);
	for (int i = 1; i < fatBPB.BPB_NumFATs; i++) {
		uint32_t other_fat_sec = (i * fatDisk.FATSz) + fat_sec;
		ide_write(DISKNO, other_fat_sec, fat_buf1, 1);
	}
	return 0;
}

void debug_print_fat_entry(uint32_t clus) {
	user_assert(!is_bad_cluster(clus));
	uint32_t entry_val;
	get_fat_entry(clus, &entry_val);
	debugf("fat entry of cluster %u: 0x%04X\n", clus, entry_val);
}

// alloc one fat free cluster by looking for entry val = 0
// but with no care about it's entry val
// the entry val should be manually set
// after calling this function to alloc
int search_and_get_fat_entry(uint32_t *pclus) {
	uint32_t clus, entry_val;
	for (clus = 2; clus < fatDisk.CountofClusters; clus++) {
		try(get_fat_entry(clus, &entry_val));
		if (entry_val == 0) {
			*pclus = clus;
			try(set_fat_entry(clus, 0xFFFF));
			try(write_fat_cluster(clus, zero_buffer, FAT_MAX_CLUS_SIZE));
			return 0;
		}
	}
	return -E_FAT_CLUSTER_FULL;
}

// Overview:
//  alloc "count" numbers of fat cluster entries
//  and assign *pclus to first cluster num
//  return 0 on success
int alloc_fat_cluster_entries(uint32_t *pclus, uint32_t count) {
	if (count == 0) {
		*pclus = 0;
		return 0;
	}
	uint32_t prev_clus, clus;
	try(search_and_get_fat_entry(&prev_clus));
	*pclus = prev_clus;
	for (int i = 1; i < count; i++) {
		try(search_and_get_fat_entry(&clus));
		try(set_fat_entry(prev_clus, clus));
		prev_clus = clus;
	}
	try(set_fat_entry(prev_clus, 0xFFFF));
	return 0;
}

// Overview:
//  expand fat cluster by "count" numbers
int expand_fat_cluster_entries(uint32_t *pclus, uint32_t count) {
	uint32_t prev_clus, clus, entry_val = 0x0;
	for (prev_clus = *pclus; 1; prev_clus = entry_val) {
		try(get_fat_entry(prev_clus, &entry_val));
		// debugf("found prev %u entry %u\n", prev_clus, entry_val);
		if (entry_val == 0xFFFF) break;
	}
	for (int i = 0; i < count; i++) {
		try(search_and_get_fat_entry(&clus));
		// debugf("alloc completed prev = %u clus = %u\n", prev_clus, clus);
		try(set_fat_entry(prev_clus, clus));
		// debugf("setting %u to %u\n", prev_clus, clus);
		prev_clus = clus;
	}
	try(set_fat_entry(prev_clus, 0xFFFF));
	return 0;
}

// Overview:
//  free the cluster entries by setting to zero
//  WARNING: won't care about disk, only change entry val
int free_fat_cluster_entries(uint32_t clus) {
	for (uint32_t entry_val = 0x0; entry_val != 0xFFFF; clus = entry_val) {
		try(get_fat_entry(clus, &entry_val));
		try(set_fat_entry(clus, 0x0));
	}
	return 0;
}
// end of fat entry managing part
// =============================================================================


// =============================================================================
//
// below is fat cluster managing part

int is_bad_cluster(uint32_t clus) {
	return (clus >= fatDisk.CountofClusters) || (clus < 2);
}

int read_disk_fat_cluster(uint32_t clus, unsigned char *buf) {
	if (clus == 0) {
		ide_read(DISKNO, fatDisk.FirstRootDirSecNum, buf, fatDisk.RootDirSectors);
		return 0;
	}
	if (is_free_cluster(clus)) {
		return -E_FAT_ACCESS_FREE_CLUS;
	}
	if (is_bad_cluster(clus)) {
		return -E_FAT_BAD_CLUSTER;
	}
	uint32_t fat_sec = fatBPB.BPB_RsvdSecCnt + fatBPB.BPB_NumFATs * fatDisk.FATSz + fatDisk.RootDirSectors + (clus - 2) * fatBPB.BPB_SecPerClus;
	ide_read(DISKNO, fat_sec, buf, fatBPB.BPB_SecPerClus);
	return 0;
}

int write_disk_fat_cluster(uint32_t clus, unsigned char *buf) {
	if (clus == 0) {
		ide_write(DISKNO, fatDisk.FirstRootDirSecNum, buf, fatDisk.RootDirSectors);
		return 0;
	}
	if (is_free_cluster(clus)) {
		return -E_FAT_ACCESS_FREE_CLUS;
	}
	if (is_bad_cluster(clus)) {
		return -E_FAT_BAD_CLUSTER;
	}
	uint32_t fat_sec = fatBPB.BPB_RsvdSecCnt + fatBPB.BPB_NumFATs * fatDisk.FATSz + fatDisk.RootDirSectors + (clus - 2) * fatBPB.BPB_SecPerClus;
	ide_write(DISKNO, fat_sec, buf, fatBPB.BPB_SecPerClus);
	return 0;
}

int read_disk_fat_clusters(uint32_t clus, unsigned char *buf) {
	if (clus == 0) {
		return read_disk_fat_cluster(0, buf);
	}
	uint32_t prev_clus, entry_val = 0x0, finished_byts = 0;
	for (prev_clus = clus; entry_val != 0xFFFF; prev_clus = entry_val) {
		try(read_disk_fat_cluster(prev_clus, buf));
		buf += fatDisk.BytsPerClus;
		try(get_fat_entry(prev_clus, &entry_val));
	}
	return 0;
}

int write_disk_fat_clusters(uint32_t clus, unsigned char *buf) {
	if (clus == 0) {
		return write_disk_fat_cluster(0, buf);
	}
	uint32_t prev_clus, entry_val = 0x0, finished_byts = 0;
	for (prev_clus = clus; entry_val != 0xFFFF; prev_clus = entry_val) {
		try(write_disk_fat_cluster(prev_clus, buf));
		buf += fatDisk.BytsPerClus;
		try(get_fat_entry(prev_clus, &entry_val));
	}
	return 0;
}

void debug_print_cluster_disk_data(uint32_t clus) {
	unsigned char buf[FAT_MAX_CLUS_SIZE];
	read_disk_fat_cluster(clus, buf);
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
	if(is_clus_mapped(clus, &va)){
		return fat_va_is_mapped(va) && fat_va_is_dirty(va);
	}
	return 0;
}

// Overview:
//  Mark this cluster as dirty (cache page has changed and needs to be written back to disk).
int fat_dirty_clus(u_int clus) {
	void *va;
	if (!is_clus_mapped(clus, &va)) {
		return -E_FAT_CLUS_UNMAPPED;
	}

	if (!fat_va_is_mapped(va)) {
		return -E_FAT_NOT_FOUND;
	}

	if (is_free_cluster(clus)) {
		return -E_FAT_ACCESS_FREE_CLUS;
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
	if (!is_clus_mapped(clus, &va)) {
		user_panic("write unmapped cluster %d", clus);
	}

	// Step2: write data to disk
	user_assert(!write_disk_fat_cluster(clus, va));
}

// Overview:
//  Make sure a particular disk cluster is loaded into memory.
//
// Post-Condition:
//  Return 0 on success, or a negative error code on error.
//
//  If pva!=0, set *pva to the address of the cluster in memory.
//
//  If isnew!=0, set *isnew to 0 if the cluster was already in memory, or
//  to 1 if the cluster was loaded off disk to satisfy this request. (Isnew
//  lets callers like file_get_block clear any memory-only fields
//  from the disk cluster when they come in off disk.)
//
int fat_read_clus(u_int clus, void **pva, u_int *isnew) {
	// Step 1: validate blockno. Make file the block to read is within the disk.
	// Step 2: validate this block is used, not free.
	if (is_free_cluster(clus)) {
		return -E_FAT_ACCESS_FREE_CLUS;
	}
	if (is_bad_cluster(clus)) {
		return -E_FAT_BAD_CLUSTER;
	}

	// Step 3: transform block number to corresponding virtual address.
	void *va;

	// Step 4: read disk and set *isnew.
	// Hint:
	//  If this cluster is already mapped, just set *isnew, else alloc memory and
	//  read data using function
	if (is_clus_mapped(clus, &va)) { // the block is in memory
		if (isnew) {
			*isnew = 0;
		}
	} else { // the block is not in memory
		if (isnew) {
			*isnew = 1;
		}
		alloc_fat_file_space(clus, fatDisk.BytsPerClus, va);
		u_int pagecnt = (fatDisk.BytsPerClus + BY2PG - 1) / BY2PG;
		for (int i = 0; i < pagecnt; i++) {
			syscall_mem_alloc(0, va + i * BY2PG, PTE_D);
		}
		read_disk_fat_cluster(clus, va);
	}

	// Step 5: if blk != NULL, assign 'va' to '*blk'.
	if (pva) {
		*pva = va;
	}
	return 0;
}

// Overview:
//  Allocate a page to cache the disk cluster.
int fat_map_clus(u_int clus) {
	// Step 1: If the cluster is already mapped in cache, return 0.
	void *va;
	if (is_clus_mapped(clus, &va)) {
		return 0;
	}

	// Step 2: Alloc a page in permission 'PTE_D' via syscall.
	// Hint: Use 'diskaddr' for the virtual address.
	/* Exercise 5.7: Your code here. (2/5) */
	u_int pagecnt = (fatDisk.BytsPerClus + BY2PG - 1) / BY2PG;
	for (int i = 0; i < pagecnt; i++) {
		try(syscall_mem_alloc(0, va + i * BY2PG, PTE_D));
	}
	return 0;
}

// Overview:
//  Unmap a disk cluster in cache.
void fat_unmap_clus(u_int clus) {
	// Step 1: Get the mapped address of the cache page of this cluster.
	void *va;
	user_assert(is_clus_mapped(clus, &va));

	// Step 2: If this block is used (not free) and dirty in cache, write it back to the disk
	// first.
	// Hint: Use 'block_is_free', 'block_is_dirty' to check, and 'write_block' to sync.
	/* Exercise 5.7: Your code here. (4/5) */

	if (fat_clus_is_dirty(clus)) {
		fat_write_clus(clus);
	}

	// Step 3: Unmap the virtual address via syscall.
	/* Exercise 5.7: Your code here. (5/5) */
	u_int pagecnt = (fatDisk.BytsPerClus + BY2PG - 1) / BY2PG;
	for (int i = 0; i < pagecnt; i++) {
		user_assert(!syscall_mem_unmap(0, va + i * BY2PG));
	}
}

void read_root(void) {
	u_int root_byte_cnt = fatDisk.RootDirSectors * fatBPB.BPB_BytsPerSec;
	void *va = FATROOTVA;
	u_int pagecnt = (root_byte_cnt + BY2PG - 1) / BY2PG;
	for (int i = 0; i < pagecnt; i++) {
		user_assert(!syscall_mem_alloc(0, va + i * BY2PG, PTE_D));
	}
	read_disk_fat_clusters(0, va);
}

// Overview:
//  Initialize the file system.
// Hint:
//  1. read super block.
//  2. check if the disk can work.
//  3. read bitmap blocks from disk to memory.
void fat_fs_init(void) {
	char fat_buf[BY2PG];
	ide_read(DISKNO, 0, fat_buf, 1);
	fatBPB = *(struct FATBPB *)fat_buf;
	user_assert(fatBPB.BS_jmpBoot[0] == 0xEB || fatBPB.BS_jmpBoot[0] == 0xE9);
	user_assert(fatBPB.BS_Reserved1 == 0x0);
	user_assert(fat_buf[510] == 0x55 && fat_buf[511] == 0xAA);
	fatDisk.RootDirSectors = (fatBPB.BPB_RootEntCnt * 32 + fatBPB.BPB_BytsPerSec - 1) / fatBPB.BPB_BytsPerSec;
	fatDisk.FATSz = fatBPB.BPB_FATSz16;
	fatDisk.TotSec = (fatBPB.BPB_TotSec16 != 0) ? fatBPB.BPB_TotSec16 : fatBPB.BPB_TotSec32;
	fatDisk.DataSec = fatDisk.TotSec - (fatBPB.BPB_RsvdSecCnt + fatBPB.BPB_NumFATs * fatDisk.FATSz + fatDisk.RootDirSectors);
	fatDisk.CountofClusters = fatDisk.DataSec / fatBPB.BPB_SecPerClus;
	fatDisk.FirstRootDirSecNum = fatBPB.BPB_RsvdSecCnt + (fatBPB.BPB_NumFATs * fatDisk.FATSz);
	fatDisk.BytsPerClus = fatBPB.BPB_BytsPerSec * fatBPB.BPB_SecPerClus;
	user_assert(fatDisk.BytsPerClus <= FAT_MAX_CLUS_SIZE);
	user_assert(fatDisk.RootDirSectors <= FAT_MAX_ROOT_SEC_NUM);
	user_assert(fatDisk.RootDirSectors * fatBPB.BPB_BytsPerSec <= FAT_MAX_ROOT_BYTES);
	read_root();
}


// // Overview:
// //  Like pgdir_walk but for files.
// //  Find the disk block number slot for the 'filebno'th block in file 'f'. Then, set
// //  '*ppdiskbno' to point to that slot. The slot will be one of the f->f_direct[] entries,
// //  or an entry in the indirect block.
// //  When 'alloc' is set, this function will allocate an indirect block if necessary.
// //
// // Post-Condition:
// //  Return 0 on success, and set *ppdiskbno to the pointer to the target block.
// //  Return -E_NOT_FOUND if the function needed to allocate an indirect block, but alloc was 0.
// //  Return -E_NO_DISK if there's no space on the disk for an indirect block.
// //  Return -E_NO_MEM if there's not enough memory for an indirect block.
// //  Return -E_INVAL if filebno is out of range (>= NINDIRECT).
// int fat_file_block_walk(struct File *f, u_int filebno, uint32_t **ppdiskbno, u_int alloc) {
// 	int r;
// 	uint32_t *ptr;
// 	uint32_t *blk;

// 	if (filebno < NDIRECT) {
// 		// Step 1: if the target block is corresponded to a direct pointer, just return the
// 		// disk block number.
// 		ptr = &f->f_direct[filebno];
// 	} else if (filebno < NINDIRECT) {
// 		// Step 2: if the target block is corresponded to the indirect block, but there's no
// 		//  indirect block and `alloc` is set, create the indirect block.
// 		if (f->f_indirect == 0) {
// 			if (alloc == 0) {
// 				return -E_NOT_FOUND;
// 			}

// 			if ((r = fat_alloc_block()) < 0) {
// 				return r;
// 			}
// 			f->f_indirect = r;
// 		}

// 		// Step 3: read the new indirect block to memory.
// 		if ((r = fat_read_block(f->f_indirect, (void **)&blk, 0)) < 0) {
// 			return r;
// 		}
// 		ptr = blk + filebno;
// 	} else {
// 		return -E_INVAL;
// 	}

// 	// Step 4: store the result into *ppdiskbno, and return 0.
// 	*ppdiskbno = ptr;
// 	return 0;
// }

// // OVerview:
// //  Set *diskbno to the disk block number for the filebno'th block in file f.
// //  If alloc is set and the block does not exist, allocate it.
// //
// // Post-Condition:
// //  Returns 0: success, < 0 on error.
// //  Errors are:
// //   -E_NOT_FOUND: alloc was 0 but the block did not exist.
// //   -E_NO_DISK: if a block needed to be allocated but the disk is full.
// //   -E_NO_MEM: if we're out of memory.
// //   -E_INVAL: if filebno is out of range.
// int fat_file_map_block(struct File *f, u_int filebno, u_int *diskbno, u_int alloc) {
// 	int r;
// 	uint32_t *ptr;

// 	// Step 1: find the pointer for the target block.
// 	if ((r = fat_file_block_walk(f, filebno, &ptr, alloc)) < 0) {
// 		return r;
// 	}

// 	// Step 2: if the block not exists, and create is set, alloc one.
// 	if (*ptr == 0) {
// 		if (alloc == 0) {
// 			return -E_NOT_FOUND;
// 		}

// 		if ((r = fat_alloc_block()) < 0) {
// 			return r;
// 		}
// 		*ptr = r;
// 	}

// 	// Step 3: set the pointer to the block in *diskbno and return 0.
// 	*diskbno = *ptr;
// 	return 0;
// }

// // Overview:
// //  Remove a block from file f. If it's not there, just silently succeed.
// int fat_file_clear_block(struct File *f, u_int filebno) {
// 	int r;
// 	uint32_t *ptr;

// 	if ((r = fat_file_block_walk(f, filebno, &ptr, 0)) < 0) {
// 		return r;
// 	}

// 	if (*ptr) {
// 		fat_free_block(*ptr);
// 		*ptr = 0;
// 	}

// 	return 0;
// }

// // Overview:
// //  Set *blk to point at the filebno'th block in file f.
// //
// // Hint: use file_map_block and read_block.
// //
// // Post-Condition:
// //  return 0 on success, and read the data to `blk`, return <0 on error.
// int fat_file_get_block(struct File *f, u_int filebno, void **blk) {
// 	int r;
// 	u_int diskbno;
// 	u_int isnew;

// 	// Step 1: find the disk block number is `f` using `file_map_block`.
// 	if ((r = fat_file_map_block(f, filebno, &diskbno, 1)) < 0) {
// 		return r;
// 	}

// 	// Step 2: read the data in this disk to blk.
// 	if ((r = fat_read_block(diskbno, blk, &isnew)) < 0) {
// 		return r;
// 	}
// 	return 0;
// }

// // Overview:
// //  Mark the offset/BY2BLK'th block dirty in file f.
// int fat_file_dirty(struct File *f, u_int offset) {
// 	int r;
// 	u_int diskbno;

// 	if ((r = fat_file_map_block(f, offset / BY2BLK, &diskbno, 0)) < 0) {
// 		return r;
// 	}

// 	return fat_dirty_block(diskbno);
// }

// // Overview:
// //  Find a file named 'name' in the directory 'dir'. If found, set *file to it.
// //
// // Post-Condition:
// //  Return 0 on success, and set the pointer to the target file in `*file`.
// //  Return the underlying error if an error occurs.
// int fat_dir_lookup(struct File *dir, char *name, struct File **file) {
// 	// int r;
// 	// Step 1: Calculate the number of blocks in 'dir' via its size.
// 	u_int nblock;
// 	/* Exercise 5.8: Your code here. (1/3) */

// 	nblock = dir->f_size / BY2BLK;

// 	// Step 2: Iterate through all blocks in the directory.
// 	for (int i = 0; i < nblock; i++) {
// 		// Read the i'th block of 'dir' and get its address in 'blk' using 'file_get_block'.
// 		void *blk;
// 		/* Exercise 5.8: Your code here. (2/3) */

// 		try(fat_file_get_block(dir, i, &blk));

// 		struct File *files = (struct File *)blk;

// 		// Find the target among all 'File's in this block.
// 		for (struct File *f = files; f < files + FILE2BLK; ++f) {
// 			// Compare the file name against 'name' using 'strcmp'.
// 			// If we find the target file, set '*file' to it and set up its 'f_dir'
// 			// field.
// 			/* Exercise 5.8: Your code here. (3/3) */

// 			if (strcmp(f->f_name, name) == 0) {
// 				*file = f;
// 				f->f_dir = dir;
// 				return 0;
// 			}
// 		}
// 	}

// 	return -E_NOT_FOUND;
// }

// // Overview:
// //  Alloc a new File structure under specified directory. Set *file
// //  to point at a free File structure in dir.
// int fat_dir_alloc_file(struct File *dir, struct File **file) {
// 	int r;
// 	u_int nblock, i, j;
// 	void *blk;
// 	struct File *f;

// 	nblock = dir->f_size / BY2BLK;

// 	for (i = 0; i < nblock; i++) {
// 		// read the block.
// 		if ((r = fat_file_get_block(dir, i, &blk)) < 0) {
// 			return r;
// 		}

// 		f = blk;

// 		for (j = 0; j < FILE2BLK; j++) {
// 			if (f[j].f_name[0] == '\0') { // found free File structure.
// 				*file = &f[j];
// 				return 0;
// 			}
// 		}
// 	}

// 	// no free File structure in exists data block.
// 	// new data block need to be created.
// 	dir->f_size += BY2BLK;
// 	if ((r = fat_file_get_block(dir, i, &blk)) < 0) {
// 		return r;
// 	}
// 	f = blk;
// 	*file = &f[0];

// 	return 0;
// }

// // Overview:
// //  Skip over slashes.
// char *fat_skip_slash(char *p) {
// 	while (*p == '/') {
// 		p++;
// 	}
// 	return p;
// }

// // Overview:
// //  Evaluate a path name, starting at the root.
// //
// // Post-Condition:
// //  On success, set *pfile to the file we found and set *pdir to the directory
// //  the file is in.
// //  If we cannot find the file but find the directory it should be in, set
// //  *pdir and copy the final path element into lastelem.
// int fat_walk_path(char *path, struct File **pdir, struct File **pfile, char *lastelem) {
// 	char *p;
// 	char name[MAXNAMELEN];
// 	struct File *dir, *file;
// 	int r;

// 	// start at the root.
// 	path = fat_skip_slash(path);
// 	file = &super->s_root;
// 	dir = 0;
// 	name[0] = 0;

// 	if (pdir) {
// 		*pdir = 0;
// 	}

// 	*pfile = 0;

// 	// find the target file by name recursively.
// 	while (*path != '\0') {
// 		dir = file;
// 		p = path;

// 		while (*path != '/' && *path != '\0') {
// 			path++;
// 		}

// 		if (path - p >= MAXNAMELEN) {
// 			return -E_BAD_PATH;
// 		}

// 		memcpy(name, p, path - p);
// 		name[path - p] = '\0';
// 		path = fat_skip_slash(path);
// 		if (dir->f_type != FTYPE_DIR) {
// 			return -E_NOT_FOUND;
// 		}

// 		if ((r = fat_dir_lookup(dir, name, &file)) < 0) {
// 			if (r == -E_NOT_FOUND && *path == '\0') {
// 				if (pdir) {
// 					*pdir = dir;
// 				}

// 				if (lastelem) {
// 					strcpy(lastelem, name);
// 				}

// 				*pfile = 0;
// 			}

// 			return r;
// 		}
// 	}

// 	if (pdir) {
// 		*pdir = dir;
// 	}

// 	*pfile = file;
// 	return 0;
// }

// // Overview:
// //  Open "path".
// //
// // Post-Condition:
// //  On success set *pfile to point at the file and return 0.
// //  On error return < 0.
// int fat_file_open(char *path, struct File **file) {
// 	return fat_walk_path(path, 0, file, 0);
// }

// // Overview:
// //  Create "path".
// //
// // Post-Condition:
// //  On success set *file to point at the file and return 0.
// //  On error return < 0.
// int fat_file_create(char *path, struct File **file) {
// 	char name[MAXNAMELEN];
// 	int r;
// 	struct File *dir, *f;

// 	if ((r = fat_walk_path(path, &dir, &f, name)) == 0) {
// 		return -E_FILE_EXISTS;
// 	}

// 	if (r != -E_NOT_FOUND || dir == 0) {
// 		return r;
// 	}

// 	if (fat_dir_alloc_file(dir, &f) < 0) {
// 		return r;
// 	}

// 	strcpy(f->f_name, name);
// 	*file = f;
// 	return 0;
// }

// // Overview:
// //  Truncate file down to newsize bytes.
// //
// //  Since the file is shorter, we can free the blocks that were used by the old
// //  bigger version but not by our new smaller self. For both the old and new sizes,
// //  figure out the number of blocks required, and then clear the blocks from
// //  new_nblocks to old_nblocks.
// //
// //  If the new_nblocks is no more than NDIRECT, free the indirect block too.
// //  (Remember to clear the f->f_indirect pointer so you'll know whether it's valid!)
// //
// // Hint: use file_clear_block.
// void fat_file_truncate(struct File *f, u_int newsize) {
// 	u_int bno, old_nblocks, new_nblocks;

// 	old_nblocks = f->f_size / BY2BLK + 1;
// 	new_nblocks = newsize / BY2BLK + 1;

// 	if (newsize == 0) {
// 		new_nblocks = 0;
// 	}

// 	if (new_nblocks <= NDIRECT) {
// 		for (bno = new_nblocks; bno < old_nblocks; bno++) {
// 			fat_file_clear_block(f, bno);
// 		}
// 		if (f->f_indirect) {
// 			fat_free_block(f->f_indirect);
// 			f->f_indirect = 0;
// 		}
// 	} else {
// 		for (bno = new_nblocks; bno < old_nblocks; bno++) {
// 			fat_file_clear_block(f, bno);
// 		}
// 	}

// 	f->f_size = newsize;
// }

// // Overview:
// //  Set file size to newsize.
// int fat_file_set_size(struct File *f, u_int newsize) {
// 	if (f->f_size > newsize) {
// 		fat_file_truncate(f, newsize);
// 	}

// 	f->f_size = newsize;

// 	if (f->f_dir) {
// 		fat_file_flush(f->f_dir);
// 	}

// 	return 0;
// }

// // Overview:
// //  Flush the contents of file f out to disk.
// //  Loop over all the blocks in file.
// //  Translate the file block number into a disk block number and then
// //  check whether that disk block is dirty. If so, write it out.
// //
// // Hint: use file_map_block, block_is_dirty, and write_block.
// void fat_file_flush(struct File *f) {
// 	// Your code here
// 	u_int nblocks;
// 	u_int bno;
// 	u_int diskno;
// 	int r;

// 	nblocks = f->f_size / BY2BLK + 1;

// 	for (bno = 0; bno < nblocks; bno++) {
// 		if ((r = fat_file_map_block(f, bno, &diskno, 0)) < 0) {
// 			continue;
// 		}
// 		if (fat_block_is_dirty(diskno)) {
// 			fat_write_block(diskno);
// 		}
// 	}
// }

// // Overview:
// //  Sync the entire file system.  A big hammer.
// void fat_fs_sync(void) {
// 	int i;
// 	for (i = 0; i < super->s_nblocks; i++) {
// 		if (fat_block_is_dirty(i)) {
// 			fat_write_block(i);
// 		}
// 	}
// }

// // Overview:
// //  Close a file.
// void fat_file_close(struct File *f) {
// 	// Flush the file itself, if f's f_dir is set, flush it's f_dir.
// 	fat_file_flush(f);
// 	if (f->f_dir) {
// 		fat_file_flush(f->f_dir);
// 	}
// }

// // Overview:
// //  Remove a file by truncating it and then zeroing the name.
// int fat_file_remove(char *path) {
// 	int r;
// 	struct File *f;

// 	// Step 1: find the file on the disk.
// 	if ((r = fat_walk_path(path, 0, &f, 0)) < 0) {
// 		return r;
// 	}

// 	// Step 2: truncate it's size to zero.
// 	fat_file_truncate(f, 0);

// 	// Step 3: clear it's name.
// 	f->f_name[0] = '\0';

// 	// Step 4: flush the file.
// 	fat_file_flush(f);
// 	if (f->f_dir) {
// 		fat_file_flush(f->f_dir);
// 	}

// 	return 0;
// }


void debug_print_fatBPB() {
	char tmp_buf[32];
	debugf("====== printing fat BPB ======\n");
	debugf("jmpBoot: 0x%02X 0x%02X 0x%02X\n", fatBPB.BS_jmpBoot[0], fatBPB.BS_jmpBoot[1], fatBPB.BS_jmpBoot[2]);
	memset(tmp_buf, 0, 32);
	memcpy(tmp_buf, fatBPB.BS_OEMName, 8);
	debugf("OEMName: %s\n", tmp_buf);
	debugf("BytsPerSec: %d\n", fatBPB.BPB_BytsPerSec);
	debugf("SecPerClus: %d\n", fatBPB.BPB_SecPerClus);
	debugf("RsvdSecCnt: %d\n", fatBPB.BPB_RsvdSecCnt);
	debugf("NumFATs: %d\n", fatBPB.BPB_NumFATs);
	debugf("RootEntCnt: %d\n", fatBPB.BPB_RootEntCnt);
	debugf("TotSec16: %d\n", fatBPB.BPB_TotSec16);
	debugf("Media: 0x%02X\n", fatBPB.BPB_Media);
	debugf("FATSz16: %d\n", fatBPB.BPB_FATSz16);
	debugf("SecPerTrk: %d\n", fatBPB.BPB_SecPerTrk);
	debugf("NumHeads: %d\n", fatBPB.BPB_NumHeads);
	debugf("HiddSec: %d\n", fatBPB.BPB_HiddSec);
	debugf("TotSec32: %d\n", fatBPB.BPB_TotSec32);
	debugf("DrvNum: 0x%02X\n", fatBPB.BS_DrvNum);
	debugf("Reserved1: %d\n", fatBPB.BS_Reserved1);
	debugf("BootSig: 0x%02X\n", fatBPB.BS_BootSig);
	debugf("VolID: %u\n", fatBPB.BS_VolID);
	memset(tmp_buf, 0, 32);
	memcpy(tmp_buf, fatBPB.BS_VolLab, 11);
	debugf("VolLab: %s\n", tmp_buf);
	memset(tmp_buf, 0, 32);
	memcpy(tmp_buf, fatBPB.BS_FilSysType, 8);
	debugf("FilSysType: %s\n", tmp_buf);
	debugf("====== end of fat BPB ======\n");
}

void debug_print_fatDisk() {
	debugf("====== printing fat Disk ======\n");
	debugf("RootDirSectors: %d\n", fatDisk.RootDirSectors);
	debugf("FATSz: %d\n", fatDisk.FATSz);
	debugf("TotSec: %d\n", fatDisk.TotSec);
	debugf("DataSec: %d\n", fatDisk.DataSec);
	debugf("CountofClusters: %d\n", fatDisk.CountofClusters);
	debugf("FirstRootDirSecNum: %d\n", fatDisk.FirstRootDirSecNum);
	debugf("====== end of fat Disk ======\n");
}

void debug_print_short_dir(struct FATDIRENT *dir) {
	debugf("========= printing fat short directory =========\n");
	debugf("dir name: ");
	for (int i = 0; i < 11; i++) debugf("%c", dir->DIR_Name[i]);
	debugf("\ndir attr: 0x%02X\n", dir->DIR_Attr);
	debugf("dir nt res: 0x%02X\n", dir->DIR_NTRes);
	debugf("dir crt time tenth: %u\n", dir->DIR_CrtTimeTenth);
	debugf("dir crt time: "); debug_print_time(dir->DIR_CrtTime);
	debugf("\ndir crt date: "); debug_print_date(dir->DIR_CrtDate);
	debugf("\ndir lst acc date: 0x%04X\n", dir->DIR_LstAccDate);
	debugf("dir fst clus hi: 0x%04X\n", dir->DIR_FstClusHI);
	debugf("dir wrt time: "); debug_print_time(dir->DIR_WrtTime);
	debugf("\ndir wrt date: "); debug_print_date(dir->DIR_WrtDate);
	debugf("\ndir fst clus lo: 0x%04X\n", dir->DIR_FstClusLO);
	debugf("dir file size: 0x%08X\n", dir->DIR_FileSize);
	debugf("corresponding long chksum : 0x%02X\n", generate_long_file_check_sum(dir->DIR_Name));
	debugf("========= end of fat short directory ===========\n");
}

void debug_print_long_dir(struct FATLONGNAME *dir) {
	debugf("========= printing fat long directory =========\n");
	debugf("order: %u\n", (dir->LDIR_Ord & (uint8_t)(~0x40)));
	debugf("dir name: ");
	for (int i = 0; i < 10; i++) debugf("%c", dir->LDIR_Name1[i]);
	for (int i = 0; i < 12; i++) debugf("%c", dir->LDIR_Name2[i]);
	for (int i = 0; i < 4; i++) debugf("%c", dir->LDIR_Name3[i]);
	debugf("\ndir attr: 0x%02X\n", dir->LDIR_Attr);
	debugf("dir type: 0x%02X\n", dir->LDIR_Type);
	debugf("dir check sum: 0x%02X\n", dir->LDIR_Chksum);
	debugf("dir fst clus lo: 0x%04X\n", dir->LDIR_FstClusLO);
	debugf("========= end of fat long directory ===========\n");
}

int debug_print_file_as_dir_entry(uint32_t clus, char *buf) {
	struct FATDIRENT *fatDir = (struct FATDIRENT *)buf;
	while (fatDir->DIR_Name[0] != 0) {
		fatDir = (struct FATDIRENT *)buf;
		if ((fatDir->DIR_Attr & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME) {
			// debug_print_short_dir(fatDir);
			debug_print_long_dir((struct FATLONGNAME *)fatDir);
		}
		else
			debug_print_short_dir(fatDir);
		buf += sizeof(struct FATDIRENT);
	}
	return 0;
}