#include <lib.h>

#include "serv.h"
#include "fat.h"

#define DISKNO 1

struct FatBPB fatBPB;
struct FatDisk fatDisk;

struct FatBPB *get_fat_BPB() {
	return &fatBPB;
}

struct FatDisk *get_fat_disk() {
	return &fatDisk;
}

unsigned char fat_buf[BY2SECT], fat_buf2[BY2SECT];

void read_array(unsigned char **buf, int len, unsigned char *ret) {
	for (int i = 0; i < len; i++) {
		ret[i] = **buf;
		(*buf)++;
	}
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

void fat_init() {
	ide_read(DISKNO, 0, fat_buf, 1);
	unsigned char *buf = (unsigned char *)fat_buf;
	read_array(&buf, 3, fatBPB.jmpBoot);
	read_array(&buf, 8, fatBPB.OEMName);
	read_little_endian(&buf, 2, &fatBPB.BytsPerSec);
	read_little_endian(&buf, 1, &fatBPB.SecPerClus);
	read_little_endian(&buf, 2, &fatBPB.RsvdSecCnt);
	read_little_endian(&buf, 1, &fatBPB.NumFATs);
	read_little_endian(&buf, 2, &fatBPB.RootEntCnt);
	read_little_endian(&buf, 2, &fatBPB.TotSec16);
	read_little_endian(&buf, 1, &fatBPB.Media);
	read_little_endian(&buf, 2, &fatBPB.FATSz16);
	read_little_endian(&buf, 2, &fatBPB.SecPerTrk);
	read_little_endian(&buf, 2, &fatBPB.NumHeads);
	read_little_endian(&buf, 4, &fatBPB.HiddSec);
	read_little_endian(&buf, 4, &fatBPB.TotSec32);
	read_little_endian(&buf, 1, &fatBPB.DrvNum);
	read_little_endian(&buf, 1, &fatBPB.Reserved1);
	read_little_endian(&buf, 1, &fatBPB.BootSig);
	read_little_endian(&buf, 4, &fatBPB.VolID);
	read_array(&buf, 11, fatBPB.VolLab);
	read_array(&buf, 8, fatBPB.FilSysType);
	buf += 448;
	read_array(&buf, 2, fatBPB.Signature_word);

	fatDisk.RootDirSectors = (fatBPB.RootEntCnt * 32 + fatBPB.BytsPerSec - 1) / fatBPB.BytsPerSec;
	fatDisk.FATSz = fatBPB.FATSz16;
	fatDisk.TotSec = (fatBPB.TotSec16 != 0) ? fatBPB.TotSec16 : fatBPB.TotSec32;
	fatDisk.DataSec = fatDisk.TotSec - (fatBPB.RsvdSecCnt + fatBPB.NumFATs * fatDisk.FATSz + fatDisk.RootDirSectors);
	fatDisk.CountofClusters = fatDisk.DataSec / fatBPB.SecPerClus;
	fatDisk.FirstRootDirSecNum = fatBPB.RsvdSecCnt + (fatBPB.NumFATs * fatDisk.FATSz);
}

void debug_print_fatBPB() {
	char tmp_buf[32];
	debugf("====== printing fat BPB ======\n");
	debugf("jmpBoot: 0x%02X 0x%02X 0x%02X\n", fatBPB.jmpBoot[0], fatBPB.jmpBoot[1], fatBPB.jmpBoot[2]);
	memset(tmp_buf, 0, 32);
	memcpy(tmp_buf, fatBPB.OEMName, 8);
	debugf("OEMName: %s\n", tmp_buf);
	debugf("BytsPerSec: %d\n", fatBPB.BytsPerSec);
	debugf("SecPerClus: %d\n", fatBPB.SecPerClus);
	debugf("RsvdSecCnt: %d\n", fatBPB.RsvdSecCnt);
	debugf("NumFATs: %d\n", fatBPB.NumFATs);
	debugf("RootEntCnt: %d\n", fatBPB.RootEntCnt);
	debugf("TotSec16: %d\n", fatBPB.TotSec16);
	debugf("Media: 0x%02X\n", fatBPB.Media);
	debugf("FATSz16: %d\n", fatBPB.FATSz16);
	debugf("SecPerTrk: %d\n", fatBPB.SecPerTrk);
	debugf("NumHeads: %d\n", fatBPB.NumHeads);
	debugf("HiddSec: %d\n", fatBPB.HiddSec);
	debugf("TotSec32: %d\n", fatBPB.TotSec32);
	debugf("DrvNum: 0x%02X\n", fatBPB.DrvNum);
	debugf("Reserved1: %d\n", fatBPB.Reserved1);
	debugf("BootSig: 0x%02X\n", fatBPB.BootSig);
	debugf("VolID: %u\n", fatBPB.VolID);
	memset(tmp_buf, 0, 32);
	memcpy(tmp_buf, fatBPB.VolLab, 11);
	debugf("VolLab: %s\n", tmp_buf);
	memset(tmp_buf, 0, 32);
	memcpy(tmp_buf, fatBPB.FilSysType, 8);
	debugf("FilSysType: %s\n", tmp_buf);
	debugf("Signature_word: 0x%02X 0x%02X\n", fatBPB.Signature_word[0], fatBPB.Signature_word[1]);
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

int is_bad_cluster(uint32_t clus) {
	return (clus >= fatDisk.CountofClusters);
}

int is_different_buffers(uint32_t n) {
	for (int i = 0; i < n; i++) {
		if (fat_buf[i] != fat_buf2[i]) {
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
	uint32_t fat_sec = fatBPB.RsvdSecCnt + (fat_offset / fatBPB.BytsPerSec);
	uint32_t fat_ent_offset = (fat_offset % fatBPB.BytsPerSec);
	ide_read(DISKNO, fat_sec, fat_buf, 1);

	// check all FATs same
	for (int i = 1; i < fatBPB.NumFATs; i++) {
		uint32_t other_fat_sec = (i * fatDisk.FATSz) + fat_sec;
		ide_read(DISKNO, other_fat_sec, fat_buf2, 1);
		if (is_different_buffers(fatBPB.BytsPerSec)) {
			return -E_FAT_DIFF;
		}
	}

	unsigned char *tmp_buf = fat_buf + fat_ent_offset;
	// debugf("reading buf = %02X %02X, offset = %u\n", tmp_buf[0], tmp_buf[1], fat_ent_offset);
	read_little_endian(&tmp_buf, 2, pentry_val);
	return 0;
}

int set_fat_entry(uint32_t clus, uint32_t entry_val) {
	if (is_bad_cluster(clus)) {
		return -E_FAT_BAD_CLUSTER;
	}
	uint32_t fat_offset = clus * 2;
	uint32_t fat_sec = fatBPB.RsvdSecCnt + fat_offset / fatBPB.BytsPerSec;
	uint32_t fat_ent_offset = fat_offset % fatBPB.BytsPerSec;
	ide_read(DISKNO, fat_sec, fat_buf, 1);

	// check all FATs same
	for (int i = 1; i < fatBPB.NumFATs; i++) {
		uint32_t other_fat_sec = (i * fatDisk.FATSz) + fat_sec;
		ide_read(DISKNO, other_fat_sec, fat_buf2, 1);
		if (is_different_buffers(fatBPB.BytsPerSec)) {
			return -E_FAT_DIFF;
		}
	}

	unsigned char *tmp_buf = fat_buf + fat_ent_offset;
	write_little_endian(&tmp_buf, 2, entry_val);
	ide_write(DISKNO, fat_sec, fat_buf, 1);
	for (int i = 1; i < fatBPB.NumFATs; i++) {
		uint32_t other_fat_sec = (i * fatDisk.FATSz) + fat_sec;
		ide_write(DISKNO, other_fat_sec, fat_buf, 1);
	}
	return 0;
}

void debug_print_fat_entry(uint32_t clus) {
	user_assert(!is_bad_cluster(clus));
	uint32_t entry_val;
	get_fat_entry(clus, &entry_val);
	debugf("fat entry of cluster %u: 0x%04X\n", clus, entry_val);
}

int read_fat_cluster(uint32_t clus, unsigned char *buf) {
	if (is_bad_cluster(clus)) {
		return -E_FAT_BAD_CLUSTER;
	}
	uint32_t fat_sec = fatBPB.RsvdSecCnt + fatBPB.NumFATs * fatDisk.FATSz + (clus - 2) * fatBPB.SecPerClus;
	ide_read(DISKNO, fat_sec, buf, fatBPB.SecPerClus);
	return 0;
}

int write_fat_cluster(uint32_t clus, unsigned char *buf) {
	if (is_bad_cluster(clus)) {
		return -E_FAT_BAD_CLUSTER;
	}
	uint32_t fat_sec = fatBPB.RsvdSecCnt + fatBPB.NumFATs * fatDisk.FATSz + (clus - 2) * fatBPB.SecPerClus;
	ide_write(DISKNO, fat_sec, buf, fatBPB.SecPerClus);
	return 0;
}

void debug_print_cluster_data(uint32_t clus) {
	unsigned char buf[16384];
	read_fat_cluster(clus, buf);
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

// alloc one fat free cluster
// but with no care about it's entry val
// the entry val should be manually set
// after calling this function to alloc
int alloc_fat_cluster(uint32_t *pclus) {
	uint32_t clus, entry_val;
	for (clus = 2; clus < fatDisk.CountofClusters; clus++) {
		try(get_fat_entry(clus, &entry_val));
		if (entry_val == 0) {
			*pclus = clus;
			try(set_fat_entry(clus, 0xFFFF));
			return 0;
		}
	}
	return -E_FAT_CLUSTER_FULL;
}

int alloc_fat_clusters(uint32_t *pclus, uint32_t count) {
	uint32_t prev_clus, clus;
	try(alloc_fat_cluster(&prev_clus));
	*pclus = prev_clus;
	for (int i = 1; i < count; i++) {
		try(alloc_fat_cluster(&clus));
		try(set_fat_entry(prev_clus, clus));
		prev_clus = clus;
	}
	try(set_fat_entry(prev_clus, 0xFFFF));
	return 0;
}

int expand_fat_clusters(uint32_t *pclus, uint32_t count) {
	uint32_t prev_clus, clus, entry_val = 0x0;
	for (prev_clus = *pclus; 1; prev_clus = entry_val) {
		try(get_fat_entry(prev_clus, &entry_val));
		// debugf("found prev %u entry %u\n", prev_clus, entry_val);
		if (entry_val == 0xFFFF) break;
	}
	for (int i = 0; i < count; i++) {
		try(alloc_fat_cluster(&clus));
		// debugf("alloc completed prev = %u clus = %u\n", prev_clus, clus);
		try(set_fat_entry(prev_clus, clus));
		// debugf("setting %u to %u\n", prev_clus, clus);
		prev_clus = clus;
	}
	try(set_fat_entry(prev_clus, 0xFFFF));
	return 0;
}

int free_fat_clusters(uint32_t clus) {
	for (uint32_t entry_val = 0x0; entry_val != 0xFFFF; clus = entry_val) {
		try(get_fat_entry(clus, &entry_val));
		try(set_fat_entry(clus, 0x0));
	}
	return 0;
}

void debug_print_short_dir(struct FatShortDir *dir) {
	debugf("========= printing fat short directory =========\n");
	debugf("dir name: ");
	for (int i = 0; i < 11; i++) debugf("%c", dir->Name[i]);
	debugf("\ndir attr: 0x%02X\n", dir->Attr);
	debugf("dir nt res: 0x%02X\n", dir->NTRes);
	debugf("dir crt time tenth: 0x%02X\n", dir->CrtTimeTenth);
	debugf("dir crt time: 0x%04X\n", dir->CrtTime);
	debugf("dir crt date: 0x%04X\n", dir->CrtDate);
	debugf("dir lst acc date: 0x%04X\n", dir->LstAccDate);
	debugf("dir fst clus hi: 0x%04X\n", dir->FstClusHI);
	debugf("dir wrt time: 0x%04X\n", dir->WrtTime);
	debugf("dir wrt date: 0x%04X\n", dir->WrtDate);
	debugf("dir fst clus lo: 0x%04X\n", dir->FstClusLO);
	debugf("dir file size: 0x%08X\n", dir->FileSize);
	debugf("corresponding long chksum : 0x%02X\n", generate_long_file_check_sum(dir->Name));
	debugf("========= end of fat short directory ===========\n");
}

void debug_print_long_dir(struct FatLongDir *dir) {
	debugf("========= printing fat long directory =========\n");
	debugf("order: %u\n", (dir->Ord & (uint8_t)(~0x40)));
	debugf("dir name: ");
	for (int i = 0; i < 10; i++) debugf("%c", dir->Name1[i]);
	for (int i = 0; i < 12; i++) debugf("%c", dir->Name2[i]);
	for (int i = 0; i < 4; i++) debugf("%c", dir->Name3[i]);
	debugf("\ndir attr: 0x%02X\n", dir->Attr);
	debugf("dir type: 0x%02X\n", dir->Type);
	debugf("dir check sum: 0x%02X\n", dir->Chksum);
	debugf("dir fst clus lo: 0x%04X\n", dir->FstClusLO);
	debugf("========= end of fat long directory ===========\n");
}

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
unsigned char generate_long_file_check_sum(unsigned char *pFcbName) {
	short FcbNameLen;
	unsigned char Sum;
	Sum = 0;
	for (FcbNameLen=11; FcbNameLen!=0; FcbNameLen--) {
		// NOTE: The operation is an unsigned char rotate right
		Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
	}
	return (Sum);
}

int read_dir() {
	ide_read(DISKNO, fatDisk.FirstRootDirSecNum, fat_buf, 1);
	unsigned char *buf = (unsigned char *)fat_buf;
	struct FatShortDir *fatDir = (struct FatShortDir *)buf;
	while (fatDir->Name[0] != 0) {
		fatDir = (struct FatShortDir *)buf;
		if ((fatDir->Attr & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME) {
			// debug_print_short_dir(fatDir);
			debug_print_long_dir((struct FatLongDir *)fatDir);
		}
		else
			debug_print_short_dir(fatDir);
		buf += sizeof(struct FatShortDir);
	}
	return 0;
}
