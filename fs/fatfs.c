#include "fatserv.h"
#include <mmu.h>

struct FATBPB fatBPB;
struct FATDISK fatDisk;
unsigned char zero_buffer[FAT_MAX_CLUS_SIZE];

struct FATDIRENT fat_root_dir_ent;

struct FATDIRENT *fat_get_root() {
	return &fat_root_dir_ent;
}

void read_little_endian(unsigned char **buf, int len, uint32_t *ret) {
	*ret = 0;
	for (int i = len - 1; i >= 0; i--) {
		*ret = (*ret << 8) + (*buf)[i];
		// debugf("ret currently = %X\n", *ret);
	}
	(*buf) += len;
}

void write_little_endian(unsigned char **buf, int len, uint32_t val) {
	for (int i = 0; i < len; i++) {
		(*buf)[i] = (val & 0xff);
		val >>= 8;
	}
	(*buf) += len;
}

int fat_get_clus_size() {
	return fatDisk.BytsPerClus;
}

int is_free_cluster(uint32_t clus);
int get_fat_entry(uint32_t clus, uint32_t *pentry_val);
int is_bad_cluster(uint32_t clus);

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
	// debugf("inserting space [0x%X, 0x%X]\n", st_va, st_va + size);
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
	if (clus == 0) {
		if (va) {
			*va = (uint32_t)FATROOTVA;
		}
		return 1;
	}
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

struct FatSpace *get_clus_space_info(uint32_t clus) {
	user_assert(!is_bad_cluster(clus));
	struct FatSpace *fspace;
	for (fspace = fat_space_head.nxt; fspace != (&fat_space_tail); fspace = fspace->nxt) {
		if (fspace->clus == clus) {
			return fspace;
		}
	}
	user_panic("can\'t find unmapped cluster %u!", clus);
	return 0;
}

// Overview:
// free the cluster space in cache
int free_clus(uint32_t clus) {
	user_assert(!is_bad_cluster(clus));
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
	user_assert(!is_bad_cluster(clus));
	bysize = ROUND(bysize, BY2PG);
	if (is_clus_mapped(clus, (uint32_t *)(&va))) {
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

void fat_space_init() {
	for (int i = 0; i < FAT_MAX_SPACE_SIZE; i++) {
		fat_spaces[i].st_va = 0;
	}

	fat_space_head.nxt = &fat_space_tail;
	fat_space_tail.prev = &fat_space_head;
	insert_head_fat_space_list(FATVAMIN, FATVARANGE, 0);
}

// end of fat space managing part
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

// userless in current management
// int read_disk_fat_clusters(uint32_t clus, unsigned char *buf) {
// 	if (clus == 0) {
// 		return read_disk_fat_cluster(0, buf);
// 	}
// 	uint32_t prev_clus, entry_val = 0x0;
// 	for (prev_clus = clus; entry_val != 0xFFFF; prev_clus = entry_val) {
// 		try(read_disk_fat_cluster(prev_clus, buf));
// 		buf += fatDisk.BytsPerClus;
// 		try(get_fat_entry(prev_clus, &entry_val));
// 	}
// 	return 0;
// }

// int write_disk_fat_clusters(uint32_t clus, unsigned char *buf) {
// 	if (clus == 0) {
// 		return write_disk_fat_cluster(0, buf);
// 	}
// 	uint32_t prev_clus, entry_val = 0x0;
// 	for (prev_clus = clus; entry_val != 0xFFFF; prev_clus = entry_val) {
// 		try(write_disk_fat_cluster(prev_clus, buf));
// 		buf += fatDisk.BytsPerClus;
// 		try(get_fat_entry(prev_clus, &entry_val));
// 	}
// 	return 0;
// }

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
// after allocing the cluster will be cleared to zero
int search_and_get_fat_entry(uint32_t *pclus) {
	uint32_t clus, entry_val;
	for (clus = 2; clus < fatDisk.CountofClusters; clus++) {
		try(get_fat_entry(clus, &entry_val));
		if (entry_val == 0) {
			*pclus = clus;
			try(set_fat_entry(clus, 0xFFFF));
			// debugf("alloced new clus %u\n", clus);
			try(write_disk_fat_cluster(clus, zero_buffer));
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
int expand_fat_cluster_entries(uint32_t *pclus, uint32_t count, uint32_t *pendclus) {
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
	if (pendclus) {
		*pendclus = prev_clus;
	}
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

// Overview:
//  shrink fat cluster by "count" numbers
int shrink_fat_cluster_entries(uint32_t *pclus, uint32_t count) {
	if (count == 0) {
		return -E_FAT_INVAL;
	}
	u_int cnt = 0;
	u_int clus = *pclus;
	for (uint32_t entry_val = 0x0; entry_val != 0xFFFF; clus = entry_val) {
		try(get_fat_entry(clus, &entry_val));
		cnt++;
	}
	if (cnt < count) {
		return -E_FAT_INVAL;
	}
	u_int remain_size = cnt - count;
	if (remain_size == 0) {
		*pclus = 0;
		return -E_FAT_SHRINK_ALL;
	}
	cnt = 0;
	clus = *pclus;
	for (uint32_t entry_val = 0x0; entry_val != 0xFFFF; clus = entry_val) {
		try(get_fat_entry(clus, &entry_val));
		cnt++;
		if (cnt == remain_size) {
			try(set_fat_entry(clus, 0xFFFF));
			free_fat_cluster_entries(entry_val);
			return 0;
		}
	}
	user_panic("shouldn\'t run to here.");
	return -E_FAT_INVAL;
}

int query_fat_clusters(uint32_t clus, uint32_t *pnum) {
	u_int cnt = 0;
	for (uint32_t entry_val = 0x0; entry_val != 0xFFFF; clus = entry_val) {
		try(get_fat_entry(clus, &entry_val));
		cnt++;
	}
	*pnum = cnt;
	return 0;
}
// end of fat entry managing part
// =============================================================================


//-----------------------------------------------------------------------------
// This is a function provided by FAT document.
// ChkSum()
// Returns an unsigned byte checksum computed on an unsigned byte
// array. The array must be 11 bytes long and is assumed to contain
// a name stored in the format of a MS-DOS directory entry.
// Passed: pFcbName Pointer to an unsigned byte array assumed to be
// 11 bytes long.
// Returns: Sum An 8-bit unsigned checksum of the array pointed
// to by pFcbName.
//------------------------------------------------------------------------------
char generate_long_file_check_sum(unsigned char *pFcbName) {
	short FcbNameLen;
	unsigned char Sum;
	Sum = 0;
	for (FcbNameLen=11; FcbNameLen!=0; FcbNameLen--) {
		// NOTE: The operation is an unsigned char rotate right
		Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
	}
	return (Sum);
}

char encode_char(char ch) {
	if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z')) return ch;
	if (ch >= 'a' && ch <= 'z') return ch - 'a' + 'A';
	// $%'-_@~`!(){}^#&
	if (ch == '$' || ch == '%' || ch == '\'' || ch == '-' || ch == '@' || ch == '~' || ch == '`' ||
			ch == '!' || ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '^' || ch == '#' ||
			ch == '&' || ch == '.') return ch;
	return '_';
}

int encode_short_name(struct FATDIRENT *sdir, char *file_name, char pad_char) {
	u_int name_len = 0, ext_len = 0;
	char *fp = file_name;
	while (*fp != '.' && *fp != '\0') fp++;
	name_len = fp - file_name;
	if (*fp == '.') {
		while (*fp != '\0') fp++;
		ext_len = (fp - file_name) - name_len - 1;
	}
	for (int i = 0; i < 11; i++) sdir->DIR_Name[i] = 0;
	if (name_len > 8 || ext_len > 3) {
		sdir->DIR_Name[6] = '~';
		sdir->DIR_Name[7] = pad_char;
	}
	for (int i = 0; i < 8; i++) {
		if (!sdir->DIR_Name[i]) sdir->DIR_Name[i] = i < name_len ? encode_char(file_name[i]) : ' ';
	}
	for (int i = 0; i < 3; i++) {
		if (!sdir->DIR_Name[i+8]) sdir->DIR_Name[i+8] = i < ext_len ? encode_char(file_name[name_len + 1 + i]) : ' ';
	}
	// debugf("name_len %u, ext_len %u\n", name_len, ext_len);
	return 0;
}

int encode_long_name(char *file_name, struct FATLONGNAME *ldirs, u_int count) {
	int is_writing_name = 1;
	ldirs = ldirs + count - 1;
	struct FATLONGNAME *stldirs = ldirs;
	user_assert(count < FAT_MAX_ENT_NUM);
	for (int n = count; n > 0; n--, ldirs--) {
		if (!is_writing_name) {
			break;
		}
		for (int i = 0; i < 10; i += 2) {
			if (!is_writing_name) {
				ldirs->LDIR_Name1[i] = 0xFF;
				ldirs->LDIR_Name1[i+1] = 0xFF;
			} else if (*file_name == '\0') {
				is_writing_name = 0;
				ldirs->LDIR_Name1[i] = 0;
				ldirs->LDIR_Name1[i+1] = 0;
			} else {
				ldirs->LDIR_Name1[i] = *file_name++;
				ldirs->LDIR_Name1[i+1] = 0;
			}
		}
		for (int i = 0; i < 12; i += 2) {
			if (!is_writing_name) {
				ldirs->LDIR_Name2[i] = 0xFF;
				ldirs->LDIR_Name2[i+1] = 0xFF;
			} else if (*file_name == '\0') {
				is_writing_name = 0;
				ldirs->LDIR_Name2[i] = 0;
				ldirs->LDIR_Name2[i+1] = 0;
			} else {
				ldirs->LDIR_Name2[i] = *file_name++;
				ldirs->LDIR_Name2[i+1] = 0;
			}
		}
		for (int i = 0; i < 4; i += 2) {
			if (!is_writing_name) {
				ldirs->LDIR_Name3[i] = 0xFF;
				ldirs->LDIR_Name3[i+1] = 0xFF;
			} else if (*file_name == '\0') {
				is_writing_name = 0;
				ldirs->LDIR_Name3[i] = 0;
				ldirs->LDIR_Name3[i+1] = 0;
			} else {
				ldirs->LDIR_Name3[i] = *file_name++;
				ldirs->LDIR_Name3[i+1] = 0;
			}
		}
	}
	return stldirs - ldirs;
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
	if(is_clus_mapped(clus, (uint32_t *)(&va))){
		return fat_va_is_mapped(va) && fat_va_is_dirty(va);
	}
	return 0;
}

// Overview:
//  Mark this cluster as dirty (cache page has changed and needs to be written back to disk).
int fat_dirty_clus(u_int clus) {
	void *va;
	if (!is_clus_mapped(clus, (uint32_t *)(&va))) {
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
	if (clus == 0) {
		user_assert(!write_disk_fat_cluster(0, (unsigned char *)FATROOTVA));
		return;
	}
	// debugf("writing clus %u\n", clus);
	void *va;
	// Step 1: detect is this block is mapped, if not, can't write it's data to disk.
	if (!is_clus_mapped(clus, (uint32_t *)(&va))) {
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
	if (is_clus_mapped(clus, (uint32_t *)(&va))) { // the block is in memory
		if (isnew) {
			*isnew = 0;
		}
	} else { // the block is not in memory
		// debugf("not mapped, allocing");
		if (isnew) {
			*isnew = 1;
		}
		alloc_fat_file_space(clus, fatDisk.BytsPerClus, (uint32_t *)(&va));
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
	if (is_clus_mapped(clus, (uint32_t *)(&va))) {
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
	user_assert(is_clus_mapped(clus, (uint32_t *)(&va)));

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
	void *va = (void *)FATROOTVA;
	u_int pagecnt = (root_byte_cnt + BY2PG - 1) / BY2PG;
	for (int i = 0; i < pagecnt; i++) {
		user_assert(!syscall_mem_alloc(0, va + i * BY2PG, PTE_D));
	}
	read_disk_fat_cluster(0, va);
}

// Overview:
//  Initialize the file system.
// Hint:
//  1. read super block.
//  2. check if the disk can work.
//  3. read bitmap blocks from disk to memory.
void fat_fs_init(void) {
	for (int i = 0; i < FAT_MAX_CLUS_SIZE; i++) zero_buffer[i] = 0;
	uint8_t fat_buf[BY2PG];
	ide_read(DISKNO, 0, fat_buf, 1);
	fatBPB = *((struct FATBPB *)fat_buf);
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
	fat_root_dir_ent.DIR_Attr = FAT_ATTR_VOLUME_ID | FAT_ATTR_DIRECTORY;
	fat_root_dir_ent.DIR_FileSize = 0;
	fat_root_dir_ent.DIR_FstClusLO = 0;
	fat_space_init();

	debug_print_fatBPB();
	debug_print_fatDisk();
}

// Overview:
//  Like pgdir_walk but for files.
//  Find the disk cluster number slot for the 'fileclno'th block in file 'ent'. Then, set
//  '*pclus' to the starting cluster of ent
//  When 'alloc' is set, this function will alloc more clusters for ent if necessary.
//
// Post-Condition:
//  Return 0 on success, and set *ppdiskbno to the pointer to the target block.
//  Return -E_FAT_NOT_FOUND if the function needed to allocate an indirect block, but alloc was 0.
int fat_file_get_cluster_by_order(struct FATDIRENT *ent, u_int fileclno, uint32_t *pclus, u_int alloc) {
	uint32_t clus = ent->DIR_FstClusLO;
	uint32_t cnt = 0;
	for (uint32_t entry_val = 0x0; entry_val != 0xFFFF; clus = entry_val) {
		// debugf("reading clus %u\n", clus);
		try(get_fat_entry(clus, &entry_val));
		if (cnt == fileclno) {
			*pclus = clus;
			return 0;
		}
		cnt++;
	}
	if (!alloc) {
		return -E_FAT_NOT_FOUND;
	}
	clus = ent->DIR_FstClusLO;
	// fileclno starts from 0
	try(expand_fat_cluster_entries(&clus, fileclno + 1 - cnt, 0));
	cnt = 0;
	for (uint32_t entry_val = 0x0; entry_val != 0xFFFF; clus = entry_val) {
		try(get_fat_entry(clus, &entry_val));
		if (cnt == fileclno) {
			*pclus = clus;
			if (entry_val != 0xFFFF) {
				user_panic("expand false happened");
			}
			return 0;
		}
		cnt++;
	}
	user_panic("bad response");
	return -1;
}

// Overview:
//  Set *diskclno to the disk cluster number for the fileclno'th block in file ent.
//  If alloc is set and the cluster does not exist, allocate it.
//
// Post-Condition:
//  Returns 0: success, < 0 on error.
//  Errors are:
//   -E_NOT_FOUND: alloc was 0 but the block did not exist.
int fat_file_map_clus(struct FATDIRENT *ent, u_int fileclno, u_int *diskclno, u_int alloc) {
	int r;
	uint32_t clus;

	// Step 1: find the pointer for the target cluster.
	if ((r = fat_file_get_cluster_by_order(ent, fileclno, &clus, alloc)) < 0) {
		return r;
	}

	// Step 2: if the block not exists, and create is set, alloc one.
	// done by fat_file_get_cluster_by_order()

	// Step 3: set the pointer to the block in *diskbno and return 0.
	*diskclno = clus;
	return 0;
}

// Overview:
//  Remove a cluster from file ent.
int fat_file_clear_clus(struct FATDIRENT *ent, u_int fileclno) {
	int r;
	uint32_t target_clus;

	if ((r = fat_file_get_cluster_by_order(ent, fileclno, &target_clus, 0)) < 0) {
		return r;
	}

	uint32_t clus = ent->DIR_FstClusLO, prev_clus = 0;
	for (uint32_t entry_val = 0x0; entry_val != 0xFFFF; clus = entry_val) {
		// debugf("reading clus %u\n", clus);
		try(get_fat_entry(clus, &entry_val));
		if (clus == target_clus) {
			if (prev_clus == 0) {
				ent->DIR_FstClusLO = (entry_val == 0xFFFF) ? 0x0 : entry_val;
			}
			else {
				try(set_fat_entry(prev_clus, entry_val));
			}
			try(set_fat_entry(clus, 0x0));
			return 0;
		}
		prev_clus = clus;
	}

	return 0;
}

// Overview:
//  look for the "fileclno"th cluster in file ent
//  and make sure it is read to disk, va = *data
//
// Post-Condition:
//  return 0 on success, and read the data to `blk`, return <0 on error.
int fat_file_get_clus(struct FATDIRENT *ent, u_int fileclno, void **data) {
	int r;
	u_int diskclno;

	// Step 1: find the disk block number is `f` using `file_map_block`.
	if ((r = fat_file_map_clus(ent, fileclno, &diskclno, 1)) < 0) {
		return r;
	}

	// Step 2: read the data in this disk to blk.
	if ((r = fat_read_clus(diskclno, data, 0)) < 0) {
		return r;
	}
	return 0;
}

// Overview:
//  Mark the offset/BytsPerClus'th cluster dirty in file ent.
int fat_file_dirty(struct FATDIRENT *ent, u_int offset) {
	int r;
	u_int diskclno;

	if ((r = fat_file_map_clus(ent, offset / fatDisk.BytsPerClus, &diskclno, 0)) < 0) {
		return r;
	}

	return fat_dirty_clus(diskclno);
}

// Overview:
//  read a dir or file to memory
int fat_read_file_to_memory(struct FATDIRENT *ent) {
	uint32_t clus = ent->DIR_FstClusLO;
	for (uint32_t entry_val = 0x0; entry_val != 0xFFFF; clus = entry_val) {
		try(get_fat_entry(clus, &entry_val));
		if (!is_clus_mapped(clus, 0)) {
			try(alloc_fat_file_space(clus, fatDisk.BytsPerClus, 0));
		}
		void *va;
		user_assert(is_clus_mapped(clus, (uint32_t *)(&va)));
		try(read_disk_fat_cluster(clus, va));
	}
	return 0;
}

int read_and_cat_long_file_name(struct FATLONGNAME *fatlDir, char *ori_name) {
	char cur_name[30];
	char *name = cur_name, *st_ori_name = ori_name;
	for (int i = 0; i < 10; i += 2) {
		if (fatlDir->LDIR_Name1[i] == (uint8_t)0xFF && fatlDir->LDIR_Name1[i+1] == (uint8_t)0xFF) goto read_long_end;
		*name++ = fatlDir->LDIR_Name1[i];
	}
	for (int i = 0; i < 12; i += 2) {
		if (fatlDir->LDIR_Name2[i] == (uint8_t)0xFF && fatlDir->LDIR_Name2[i+1] == (uint8_t)0xFF) goto read_long_end;
		*name++ = fatlDir->LDIR_Name2[i];
	}
	for (int i = 0; i < 4; i += 2) {
		if (fatlDir->LDIR_Name3[i] == (uint8_t)0xFF && fatlDir->LDIR_Name3[i+1] == (uint8_t)0xFF) goto read_long_end;
		*name++ = fatlDir->LDIR_Name3[i];
	}
read_long_end:;
		*name = '\0';
		// debugf("got ori name %s and new name %s\n", st_ori_name, cur_name);
		int add_length = name - cur_name;
		while (*ori_name != '\0')ori_name++;
		*(ori_name + add_length) = *ori_name;
		while (ori_name != st_ori_name) {
			ori_name--;
			*(ori_name + add_length) = *ori_name;
		}
		for (name = cur_name; name - cur_name < add_length;) {
			*ori_name++ = *name++;
		}
		// debugf("output name %s\n", st_ori_name);
		return add_length;
}

// Overview:
// input the first Fat directory entry pointer
// (if long name file, then give the first long name entry)
// and return the file name in buf
// the next directory entry pointer will be stored at *nxtdir
// we can't handle when long name entries are crossing cluster
int fat_get_full_name(struct FATDIRENT *dir, char *buf, struct FATDIRENT **nxtdir) {
	if (dir->DIR_Name[0] == (char)FAT_DIR_ENTRY_FREE) {
		return -E_FAT_READ_FREE_DIR;
	}
	if ((dir->DIR_Attr & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME) {
		if ((dir->DIR_Name[0] & FAT_LAST_LONG_ENTRY) != FAT_LAST_LONG_ENTRY) {
			// input long name entry isn't the first one
			return -E_FAT_BAD_DIR;
		}
		*buf = '\0';
		struct FATLONGNAME *ldir;
		for (ldir = (struct FATLONGNAME *)dir; (ldir->LDIR_Attr & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME; ldir++) {
			read_and_cat_long_file_name(ldir, buf);
		}
		*nxtdir = (struct FATDIRENT *)ldir + 1;
	}
	else {
		char *tmpbuf_ptr = buf;
		for (int i = 0; i < 11; i++) {
			if (dir->DIR_Name[i] == '\0' || dir->DIR_Name[i] == ' ') continue;
			if (i == 8) *tmpbuf_ptr++ = '.';
			*tmpbuf_ptr++ = dir->DIR_Name[i];
		}
		*tmpbuf_ptr++ = '\0';
		*nxtdir = dir + 1;
	}
	return 0;
}

// Overview:
//  Find a file named 'name' in the directory 'dir'. If found, set *ent to it.
//
// Post-Condition:
//  Return 0 on success, and set the pointer to the target file in `*file`.
//  Return the corresponding error if an error occurs.
// 	If pclus is set, set *pclus to where ent is in
int fat_dir_lookup(struct FATDIRENT *dir, char *name, struct FATDIRENT **ent, uint32_t *pclus) {
	char encoded_name[20];
	encoded_name[0] = '\0';
	if (strlen(name) <= 12) {
		int i;
		for (i = 0; name[i] != '\0'; i++) encoded_name[i] = encode_char(name[i]);
		encoded_name[i] = '\0';
	}

	uint32_t clus = dir->DIR_FstClusLO;
	for (uint32_t entry_val = 0x0; entry_val != 0xFFFF; clus = entry_val) {
		if (clus == 0) {
			// reading root
			user_assert(entry_val == 0x0);
			entry_val = 0xFFFF;
		}
		else {
			try(get_fat_entry(clus, &entry_val));
		}

		void *va;
		if (!is_clus_mapped(clus, (uint32_t *)(&va))) {
			try(alloc_fat_file_space(clus, fatDisk.BytsPerClus, (uint32_t *)(&va)));
			// we assume all mapped cluster is read to memory
			try(read_disk_fat_cluster(clus, va));
		}

		u_int max_ent_cnt = (va == (void *)FATROOTVA) ? fatBPB.BPB_RootEntCnt : (fatDisk.BytsPerClus / BY2DIRENT);

		struct FATDIRENT *ient;
		struct FATDIRENT *stent = (struct FATDIRENT *)va;
		char name_buf[MAXNAMELEN];
		for (ient = stent; ient - stent < max_ent_cnt; ient++) {
			if (ient->DIR_Name[0] == 0x0) {
				break;
			}
			if (ient->DIR_Name[0] == FAT_DIR_ENTRY_FREE) {
				continue;
			}
			struct FATDIRENT *nxtent;
			try(fat_get_full_name(ient, name_buf, &nxtent));
			// debugf("found name %s encoded name %s\n", name_buf, encoded_name);
			if (strcmp(name_buf, name) == 0 || strcmp(name_buf, encoded_name) == 0) {
				*ent = nxtent - 1;
				if (pclus) {
					*pclus = clus;
				}
				return 0;
			}
			ient = nxtent - 1;
		}
	}

	return -E_FAT_NOT_FOUND;
}

// Overview:
//  Alloc "count" number of new FATDIRENT structures under specified directory. 
// 	we must give continue entries so file will be set to first start of 
//	a set of entries that satisfy our request
int fat_dir_alloc_files(struct FATDIRENT *dir, struct FATDIRENT **file, u_int count) {
	// we can't handle cross-cluster long name dirs
	user_assert(count <= (fatDisk.BytsPerClus / BY2DIRENT));

	uint32_t clus = dir->DIR_FstClusLO;
	for (uint32_t entry_val = 0x0; entry_val != 0xFFFF; clus = entry_val) {
		if (clus == 0) {
			// reading root
			user_assert(entry_val == 0x0);
			entry_val = 0xFFFF;
		}
		else {
			try(get_fat_entry(clus, &entry_val));
		}

		void *va;
		if (!is_clus_mapped(clus, (uint32_t *)(&va))) {
			try(alloc_fat_file_space(clus, fatDisk.BytsPerClus, (uint32_t *)(&va)));
			// we assume all mapped cluster is read to memory
			try(read_disk_fat_cluster(clus, va));
		}

		u_int max_ent_cnt = (va == (void *)FATROOTVA) ? fatBPB.BPB_RootEntCnt : (fatDisk.BytsPerClus / BY2DIRENT);

		struct FATDIRENT *ient;
		struct FATDIRENT *stent = (struct FATDIRENT *)va;
		u_int continuous_cnt = 0;
		for (ient = stent; ient - stent < max_ent_cnt; ient++) {
			if (ient->DIR_Name[0] == 0x0) {
				if (max_ent_cnt - (ient - stent) + continuous_cnt >= count) {
					*file = ient - continuous_cnt;
					return 0;
				}
				else {
					// if rest space can't satisfy our request, we set all dir remain
					// as empty and alloc a new cluster
					ient->DIR_Name[0] = FAT_DIR_ENTRY_FREE;
					continue;
				}
			}
			if (ient->DIR_Name[0] == FAT_DIR_ENTRY_FREE) {
				continuous_cnt++;
				if (continuous_cnt >= count) {
					*file = ient - continuous_cnt + 1;
					return 0;
				}
			}
			else {
				continuous_cnt = 0;
			}
		}
	}

	// cannot find any satisfying part, alloc a new clusters
	clus = dir->DIR_FstClusLO;
	u_int new_clus;
	try(expand_fat_cluster_entries(&clus, 1, &new_clus));
	void *va;
	try(alloc_fat_file_space(new_clus, fatDisk.BytsPerClus, (uint32_t *)(&va)));
	*file = (struct FATDIRENT *)va;

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
//	if pclus is set, *pclus is set to where file is in
int fat_walk_path(char *path, struct FATDIRENT **pdir, struct FATDIRENT **pent, char *lastelem, uint32_t *pclus) {
	char *p;
	char name[MAXNAMELEN];
	struct FATDIRENT *dir, *ent;
	int r;

	// start at the root.
	path = fat_skip_slash(path);
	ent = &fat_root_dir_ent;
	dir = 0;
	name[0] = 0;

	if (pdir) {
		*pdir = 0;
	}

	*pent = 0;

	// find the target file by name recursively.
	while (*path != '\0') {
		dir = ent;
		p = path;

		while (*path != '/' && *path != '\0') {
			path++;
		}

		if (path - p >= MAXNAMELEN) {
			return -E_FAT_BAD_PATH;
		}

		memcpy(name, p, path - p);
		name[path - p] = '\0';
		path = fat_skip_slash(path);
		if ((dir->DIR_Attr & FAT_ATTR_DIRECTORY) != FAT_ATTR_DIRECTORY) {
			return -E_FAT_NOT_FOUND;
		}

		if ((r = fat_dir_lookup(dir, name, &ent, pclus)) < 0) {
			if (r == -E_FAT_NOT_FOUND && *path == '\0') {
				if (pdir) {
					*pdir = dir;
				}

				if (lastelem) {
					strcpy(lastelem, name);
				}

				*pent = 0;
			}

			return r;
		}
	}

	if (pdir) {
		*pdir = dir;
	}

	*pent = ent;
	return 0;
}

// Overview:
//  Open "path".
//
// Post-Condition:
//  On success set *pfile to point at the file and return 0.
//  On error return < 0.
int fat_file_open(char *path, struct FATDIRENT **file, struct FATDIRENT **dir) {
	return fat_walk_path(path, dir, file, 0, 0);
}

// Overview:
//  Create a file or directory by which "path" specifies.
//	But only if the prev part of "path" exists, from the front to the last slash
//	This function will set correct create time and set other times same as create time
//	Other fields in an entry like Attr and FstClusLO will all be set to 0
//	which means you need to set them yourself after calling this function
//	This function only modifies the directory entries, no alloc will be done
//	We support long name, and this function will always create file/dir as long name
//	Whether this file is a dir or not depend on how you set the rest of the entry values
//
// Post-Condition:
//  On success set *pent to point at the file, if pdir is set, set *pdir to the dir and return 0.
//  On error return < 0.
int fat_file_create(char *path, struct FATDIRENT **pent, struct FATDIRENT **pdir, uint32_t attr, uint32_t size) {
	char name[MAXNAMELEN];
	int r;
	struct FATDIRENT *dir, *stent, *sdir;
	struct FATLONGNAME *ldirs;
	uint32_t clus_num = (size + fatDisk.BytsPerClus - 1) / fatDisk.BytsPerClus;
	// debugf("creating file clus num = %u\n", clus_num);

	if ((attr & FAT_ATTR_DIRECTORY) == FAT_ATTR_DIRECTORY) {
		user_assert(size >= 2 * BY2DIRENT);
		size = 0;
	}

	if ((attr & FAT_ATTR_VOLUME_ID) != 0) {
		return -E_FAT_INVAL;
	}

	if ((r = fat_walk_path(path, &dir, &stent, name, 0)) == 0) {
		return -E_FAT_FILE_EXISTS;
	}

	if (r != -E_FAT_NOT_FOUND || dir == 0) {
		return r;
	}

	uint32_t ldir_cnt = (strlen(name) + FAT_LONG_NAME_CHAR_CNT - 1) / FAT_LONG_NAME_CHAR_CNT;

	if (fat_dir_alloc_files(dir, &stent, ldir_cnt + 1) < 0) {
		return r;
	}

	sdir = stent + ldir_cnt;
	ldirs = (struct FATLONGNAME *)stent;

	encode_short_name(sdir, name, '1');
	sdir->DIR_NTRes = 0;
	sdir->DIR_Attr = attr;
	sdir->DIR_FstClusHI = 0;
	
	u_int clus;
	alloc_fat_cluster_entries(&clus, clus_num);
	sdir->DIR_FstClusLO = clus;

	sdir->DIR_FileSize = size;
	uint32_t year, month, date, hour, minute, second, timestamp, timeus;
	timestamp = get_time(&timeus);
	try(get_all_time(timestamp, &year, &month, &date, &hour, &minute, &second));
	hour += 8; // This is UTC, we need CST
	get_fat_time(year, month, date, hour, minute, second, timeus, 
							 &sdir->DIR_CrtTimeTenth, &sdir->DIR_CrtTime, &sdir->DIR_CrtDate);
	sdir->DIR_LstAccDate = sdir->DIR_CrtDate;
	sdir->DIR_WrtTime = sdir->DIR_CrtTime;
	sdir->DIR_WrtDate = sdir->DIR_CrtDate;

	// debugf("encode = %u ldir_cnt = %u\n", encode_long_name(name, ldirs), ldir_cnt);
	encode_long_name(name, ldirs, ldir_cnt);
	uint8_t check_sum = generate_long_file_check_sum(sdir->DIR_Name);
	for (int i = 0; i < ldir_cnt; i++) {
		(ldirs + i)->LDIR_Attr = FAT_ATTR_LONG_NAME;
		(ldirs + i)->LDIR_Chksum = check_sum;
		(ldirs + i)->LDIR_FstClusLO = 0;
		(ldirs + i)->LDIR_Type = 0;
		(ldirs + i)->LDIR_Ord = (i == 0) ? ((ldir_cnt - i) | FAT_LAST_LONG_ENTRY) : ldir_cnt - i;
	}

	*pent = sdir;
	if (pdir) {
		*pdir = dir;
	}

	fat_file_flush(dir, 1);

	if ((r = fat_walk_path(path, &dir, &stent, name, 0)) < 0) {
		user_assert("create failed");
	}
	// debugf("sdir = %u, stent = %u, stent + ldir_cnt = %u\n", sdir, stent, stent + ldir_cnt);
	user_assert(sdir == stent);

	if ((attr & FAT_ATTR_DIRECTORY) == FAT_ATTR_DIRECTORY) {
		struct FATDIRENT *childdot, *childdoubledot;
		if (fat_dir_alloc_files(sdir, &childdot, 1) < 0) {
			user_assert("can\'t alloc in the new dir");
		}
		*childdot = *sdir;
		childdot->DIR_Name[0] = '.';
		for (int i = 1; i < 11; i++) {
			childdot->DIR_Name[i] = ' ';
		}
		if (fat_dir_alloc_files(sdir, &childdoubledot, 1) < 0) {
			user_assert("can\'t alloc in the new dir");
		}
		*childdoubledot = *dir;
		childdoubledot->DIR_Name[0] = '.';
		childdoubledot->DIR_Name[1] = '.';
		for (int i = 2; i < 11; i++) {
			childdoubledot->DIR_Name[i] = ' ';
		}
		fat_file_flush(sdir, 1);
	}
	return 0;
}

// Overview:
//  Truncate file "ent" down to newsize bytes.
//
//  Since the file is shorter, we can free the clusters that were used by the old
//  bigger version but not by our new smaller self. For both the old and new sizes,
//  figure out the number of blocks required, and then clear the blocks from
//  new_nblocks to old_nblocks.
void fat_file_truncate(struct FATDIRENT *ent, u_int newsize) {
	// You can't truncate the root directory
	user_assert(!is_bad_cluster(ent->DIR_FstClusLO));
	// Calling this must make the file shorter
	user_assert(newsize < ent->DIR_FileSize);

	u_int new_clus_num = (newsize + fatDisk.BytsPerClus - 1) / fatDisk.BytsPerClus;
	u_int old_clus_num = (ent->DIR_FileSize + fatDisk.BytsPerClus - 1) / fatDisk.BytsPerClus;
	if (new_clus_num == old_clus_num) {
		// cluster number won't be changed
		ent->DIR_FileSize = newsize;
		return;
	}

	u_int clus = ent->DIR_FstClusLO;
	user_assert(!shrink_fat_cluster_entries(&clus, old_clus_num - new_clus_num));

	ent->DIR_FileSize = newsize;
}

// Overview:
//  Set file size to newsize.
// 	If allocation is needed, do it.
int fat_file_set_size(struct FATDIRENT *ent, u_int newsize, struct FATDIRENT *dir) {
	// ent cannot be root
	user_assert((ent->DIR_Attr & FAT_ATTR_VOLUME_ID) == 0);

	if (ent->DIR_FileSize > newsize) {
		fat_file_truncate(ent, newsize);
	}
	else {
		u_int new_clus_num = (newsize + fatDisk.BytsPerClus - 1) / fatDisk.BytsPerClus;
		u_int old_clus_num = (ent->DIR_FileSize + fatDisk.BytsPerClus - 1) / fatDisk.BytsPerClus;
		if (new_clus_num > old_clus_num) {
			u_int clus = ent->DIR_FileSize;
			if (ent->DIR_FileSize == 0) {
				user_assert(ent->DIR_FstClusLO == 0);
				alloc_fat_cluster_entries(&clus, new_clus_num);
				ent->DIR_FstClusLO = clus;
			}
			else {
				expand_fat_cluster_entries(&clus, new_clus_num - old_clus_num, 0);
			}
		}
	}

	ent->DIR_FileSize = newsize;

	fat_file_flush(dir, 1);

	return 0;
}

// Overview:
//  Flush the contents of file ent out to disk.
//  Loop over all the clusters in file.
//  check whether that disk cluster is dirty. If so, write it out.
// 	if force is set, won't care about dirty or not.
void fat_file_flush(struct FATDIRENT *ent, u_int force) {
	if (ent->DIR_Attr & FAT_ATTR_VOLUME_ID) {
		fat_write_clus(0);
		return;
	}
	u_int clus = ent->DIR_FstClusLO;
	for (uint32_t entry_val = 0x0; entry_val != 0xFFFF; clus = entry_val) {
		user_assert(!get_fat_entry(clus, &entry_val));
		if (fat_clus_is_dirty(clus) || force) {
			fat_write_clus(clus);
		}
	}
}

// Overview:
//  Sync the entire file system.  A big hammer.
void fat_fs_sync(void) {
	u_int clus;
	fat_write_clus(0);
	for (clus = 2; clus < fatDisk.CountofClusters; clus++) {
		fat_write_clus(clus);
	}
}

// Overview:
//  Close a file. flush this file and if dir is set, flush dir.
void fat_file_close(struct FATDIRENT *ent, struct FATDIRENT *dir) {
	fat_file_flush(ent, 1);
	if (dir) {
		fat_file_flush(dir, 1);
	}
}

// Overview:
//  Remove a file.
// 	Can actually remove anything including file and directory.
int fat_file_remove(char *path) {
	struct FATDIRENT *ent, *dir;
	uint32_t clus_pos;

	// Step 1: find the file on the disk.
	try(fat_walk_path(path, &dir, &ent, 0, &clus_pos));

	u_int clus = ent->DIR_FstClusLO;
	// Step 2: free it's clusters.
	try(free_fat_cluster_entries(clus));

	// Step 3: clear it's name.
	ent->DIR_Name[0] = FAT_DIR_ENTRY_FREE;
	ent--;
	while ((ent->DIR_Attr & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME) {
		ent->DIR_Name[0] = FAT_DIR_ENTRY_FREE;
		ent--;
	}

	// Step 3.5: set dirty
	fat_dirty_clus(clus_pos);

	// Step 4: flush the directory.
	fat_file_flush(dir, 1);
	debugf("removed!\n");
	return 0;
}

void debug_print_fatBPB() {
	char tmp_buf[32];
	debugf("====== printing fat BPB ======\n");
	debugf("jmpBoot: 0x%02X 0x%02X 0x%02X\n", fatBPB.BS_jmpBoot[0], fatBPB.BS_jmpBoot[1], fatBPB.BS_jmpBoot[2]);
	memset(tmp_buf, 0, 32);
	memcpy(tmp_buf, fatBPB.BS_OEMName, 8);
	debugf("OEMName: %s\n", tmp_buf);
	debugf("OEMName: ");
	for (int i = 0; i < 8; i++) debugf("0x%02X ", fatBPB.BS_OEMName[i]);
	debugf("\n");
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

void debug_print_short_dir(struct FATDIRENT *dir, uint32_t num){
	if (dir->DIR_Name[0] == 0x0) {
		debugf("========= end of all short directories ================\n");
		return;
	}
	if (dir->DIR_Name[0] == FAT_DIR_ENTRY_FREE) {
		debugf("========= empty fat short directory No.%03d============\n", num);
		return;
	}
	debugf("========= printing fat short directory No.%03d=========\n", num);
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
	debugf("\ndir fst clus lo: 0x%04X = %d\n", dir->DIR_FstClusLO, dir->DIR_FstClusLO);
	debugf("dir file size: 0x%08X\n", dir->DIR_FileSize);
	debugf("corresponding long chksum : 0x%02X\n", (uint8_t)generate_long_file_check_sum(dir->DIR_Name));
	debugf("========= end of fat short directory ==================\n");
}

void debug_print_long_dir(struct FATLONGNAME *dir) {
	if (dir->LDIR_Ord == FAT_DIR_ENTRY_FREE) {
		debugf("========= empty fat short directory ============\n");
		return;
	}
	debugf("========= printing fat long directory =========\n");
	// debugf("order: %u\n", (dir->LDIR_Ord & (uint8_t)(~0x40)));
	debugf("order: 0x%02X\n", dir->LDIR_Ord);
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

int debug_print_file_as_dir_entry(char *buf) {
	struct FATDIRENT *fatDir = (struct FATDIRENT *)buf;
	uint32_t cnt = 0;
	while (fatDir->DIR_Name[0] != 0) {
		fatDir = (struct FATDIRENT *)buf;
		if ((fatDir->DIR_Attr & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME) {
			// debug_print_short_dir(fatDir);
			debug_print_long_dir((struct FATLONGNAME *)fatDir);
		}
		else {
			debug_print_short_dir(fatDir, cnt);
			cnt++;
		}
		buf += sizeof(struct FATDIRENT);
	}
	return 0;
}

void debug_print_fatsec(uint32_t secno) {
	unsigned char buf[1024];
	ide_read(DISKNO, secno, buf, 1);
	debugf("========= printing fat section %u =========\n", secno);
	for (int i = 0; i < 32; i++) {
		debugf("0x%4x-0x%4x: ", i*16, i*16+15);
		for (int j = 0; j < 16; j++) {
			debugf("%02X ", buf[i*16+j]);
		}
		debugf("\n");
	}
	debugf("========= end of fat section %u ===========\n", secno);
}