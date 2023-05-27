#include <lib.h>

#include "serv.h"
#include "fat.h"

#define DISKNO 1

struct FatBPB fatBPB;
struct FatDisk fatDisk;
unsigned char zero_buffer[FAT_MAX_CLUS_SIZE];

struct FatBPB *get_fat_BPB() {
	return &fatBPB;
}

struct FatDisk *get_fat_disk() {
	return &fatDisk;
}

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

u_int get_fat_time(uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute, uint32_t second, uint32_t us, uint8_t *CrtTimeTenth, uint16_t *CrtTime, uint16_t *CrtDate) {
    *CrtTimeTenth = us / 10000 + (second % 2) * 100;
    *CrtTime = (hour << 11) + (minute << 5) + (second >> 1);
    *CrtDate = ((year - 1980) << 9) + (month << 5) + day;
    return 0;
}

void fat_init() {
	unsigned char fat_buf[BY2SECT];
	ide_read(DISKNO, 0, fat_buf, 1);
	unsigned char *buf = (unsigned char *)fat_buf;
	read_array(&buf, 3, fatBPB.jmpBoot);
	read_array(&buf, 8, fatBPB.OEMName);
	read_little_endian(&buf, 2, &fatBPB.BytsPerSec);
	read_little_endian(&buf, 1, &fatBPB.SecPerClus);
	user_assert(fatBPB.BytsPerSec * fatBPB.SecPerClus <= FAT_MAX_CLUS_SIZE);
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
	user_assert(fatBPB.Signature_word[0] == 0x55 && fatBPB.Signature_word[1] == 0xAA);

	fatDisk.RootDirSectors = (fatBPB.RootEntCnt * 32 + fatBPB.BytsPerSec - 1) / fatBPB.BytsPerSec;
	user_assert(fatDisk.RootDirSectors * fatBPB.BytsPerSec <= FAT_MAX_FILE_SIZE);
	fatDisk.FATSz = fatBPB.FATSz16;
	fatDisk.TotSec = (fatBPB.TotSec16 != 0) ? fatBPB.TotSec16 : fatBPB.TotSec32;
	fatDisk.DataSec = fatDisk.TotSec - (fatBPB.RsvdSecCnt + fatBPB.NumFATs * fatDisk.FATSz + fatDisk.RootDirSectors);
	fatDisk.CountofClusters = fatDisk.DataSec / fatBPB.SecPerClus;
	fatDisk.FirstRootDirSecNum = fatBPB.RsvdSecCnt + (fatBPB.NumFATs * fatDisk.FATSz);

	memset(zero_buffer, 0, FAT_MAX_CLUS_SIZE);
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
	uint32_t fat_sec = fatBPB.RsvdSecCnt + (fat_offset / fatBPB.BytsPerSec);
	uint32_t fat_ent_offset = (fat_offset % fatBPB.BytsPerSec);

	unsigned char fat_buf1[BY2SECT], fat_buf2[BY2SECT];
	ide_read(DISKNO, fat_sec, fat_buf1, 1);

	// check all FATs same
	for (int i = 1; i < fatBPB.NumFATs; i++) {
		uint32_t other_fat_sec = (i * fatDisk.FATSz) + fat_sec;
		ide_read(DISKNO, other_fat_sec, fat_buf2, 1);
		if (is_different_buffers(fatBPB.BytsPerSec, fat_buf1, fat_buf2)) {
			return -E_FAT_DIFF;
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
	uint32_t fat_sec = fatBPB.RsvdSecCnt + fat_offset / fatBPB.BytsPerSec;
	uint32_t fat_ent_offset = fat_offset % fatBPB.BytsPerSec;
	unsigned char fat_buf1[BY2SECT], fat_buf2[BY2SECT];
	ide_read(DISKNO, fat_sec, fat_buf1, 1);

	// check all FATs same
	for (int i = 1; i < fatBPB.NumFATs; i++) {
		uint32_t other_fat_sec = (i * fatDisk.FATSz) + fat_sec;
		ide_read(DISKNO, other_fat_sec, fat_buf2, 1);
		if (is_different_buffers(fatBPB.BytsPerSec, fat_buf1, fat_buf2)) {
			return -E_FAT_DIFF;
		}
	}

	unsigned char *tmp_buf = fat_buf1 + fat_ent_offset;
	write_little_endian(&tmp_buf, 2, entry_val);
	ide_write(DISKNO, fat_sec, fat_buf1, 1);
	for (int i = 1; i < fatBPB.NumFATs; i++) {
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

int read_fat_cluster(uint32_t clus, unsigned char *buf) {
	if (clus == 0) {
		ide_read(DISKNO, fatDisk.FirstRootDirSecNum, buf, fatDisk.RootDirSectors);
		return 0;
	}
	if (is_bad_cluster(clus)) {
		return -E_FAT_BAD_CLUSTER;
	}
	if (is_free_cluster(clus)) {
		return -E_FAT_ACCESS_FREE_CLUSTER;
	}
	uint32_t fat_sec = fatBPB.RsvdSecCnt + fatBPB.NumFATs * fatDisk.FATSz + fatDisk.RootDirSectors + (clus - 2) * fatBPB.SecPerClus;
	ide_read(DISKNO, fat_sec, buf, fatBPB.SecPerClus);
	return 0;
}

int write_fat_cluster(uint32_t clus, unsigned char *buf, uint32_t nbyts) {
	if (clus == 0) {
		ide_write(DISKNO, fatDisk.FirstRootDirSecNum, buf, fatDisk.RootDirSectors);
		return 0;
	}
	if (is_bad_cluster(clus)) {
		return -E_FAT_BAD_CLUSTER;
	}
	if (is_free_cluster(clus)) {
		return -E_FAT_ACCESS_FREE_CLUSTER;
	}
	unsigned char tmp_buf[FAT_MAX_CLUS_SIZE];
	if (nbyts > fatBPB.SecPerClus * fatBPB.BytsPerSec) {
		nbyts = fatBPB.SecPerClus * fatBPB.BytsPerSec;
	}
	uint32_t fat_sec = fatBPB.RsvdSecCnt + fatBPB.NumFATs * fatDisk.FATSz + fatDisk.RootDirSectors + (clus - 2) * fatBPB.SecPerClus;
	ide_read(DISKNO, fat_sec, tmp_buf, fatBPB.SecPerClus);
	for (int i = 0; i < nbyts; i++) tmp_buf[i] = buf[i];
	ide_write(DISKNO, fat_sec, tmp_buf, fatBPB.SecPerClus);
	return 0;
}

int read_fat_clusters(uint32_t clus, unsigned char *buf, uint32_t nbyts) {
	if (clus == 0) {
		return read_fat_cluster(0, buf);
	}
	uint32_t prev_clus, entry_val = 0x0, finished_byts = 0;
	for (prev_clus = clus; entry_val != 0xFFFF; prev_clus = entry_val) {
		try(get_fat_entry(prev_clus, &entry_val));
		// debugf("trying reading cluster %u\n", prev_clus);
		try(read_fat_cluster(prev_clus, buf));
		finished_byts += fatBPB.BytsPerSec * fatBPB.SecPerClus;
		// debugf("finished byts %u total %u\n", finished_byts, nbyts);
		if (finished_byts >= nbyts)break;
		buf += finished_byts;
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
		buf += finished_byts;
	}
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

int query_fat_clusters(uint32_t st_clus, uint32_t *size) {
	uint32_t clus, entry_val;
	*size = 0;
	for (clus = st_clus; clus != 0xFFFF; clus = entry_val) {
		try(get_fat_entry(clus, &entry_val));
		(*size)++;
	}
	return 0;
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
			try(write_fat_cluster(clus, zero_buffer, FAT_MAX_CLUS_SIZE));
			return 0;
		}
	}
	return -E_FAT_CLUSTER_FULL;
}

int alloc_fat_clusters(uint32_t *pclus, uint32_t count) {
	if (count == 0) {
		*pclus = 0;
		return 0;
	}
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

void debug_print_date(uint16_t date) {
	debugf("%04u-%02u-%02u", ((date & 0xFE00) >> 9) + 1980, (date & 0x1E0) >> 5, (date & 0x1F));
}

void debug_print_time(uint16_t time) {
	debugf("%02u:%02u:%02u", (time & 0xF800) >> 11, (time & 0x7E0) >> 5, (time & 0x1F) * 2);
}

void debug_print_short_dir(struct FatShortDir *dir) {
	debugf("========= printing fat short directory =========\n");
	debugf("dir name: ");
	for (int i = 0; i < 11; i++) debugf("%c", dir->Name[i]);
	debugf("\ndir attr: 0x%02X\n", dir->Attr);
	debugf("dir nt res: 0x%02X\n", dir->NTRes);
	debugf("dir crt time tenth: %u\n", dir->CrtTimeTenth);
	debugf("dir crt time: "); debug_print_time(dir->CrtTime);
	debugf("\ndir crt date: "); debug_print_date(dir->CrtDate);
	debugf("\ndir lst acc date: 0x%04X\n", dir->LstAccDate);
	debugf("dir fst clus hi: 0x%04X\n", dir->FstClusHI);
	debugf("dir wrt time: "); debug_print_time(dir->WrtTime);
	debugf("\ndir wrt date: "); debug_print_date(dir->WrtDate);
	debugf("\ndir fst clus lo: 0x%04X\n", dir->FstClusLO);
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

int debug_print_file_as_dir_entry(uint32_t clus) {
	unsigned char fat_buf[FAT_MAX_FILE_SIZE];
	try(read_fat_clusters(clus, fat_buf, FAT_MAX_FILE_SIZE));
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

int read_and_cat_long_file_name(struct FatLongDir *fatlDir, unsigned char *ori_name) {
	unsigned char cur_name[30];
	unsigned char *name = cur_name, *st_ori_name = ori_name;
	for (int i = 0; i < 10; i += 2) {
		if (fatlDir->Name1[i] == (uint8_t)0xFF && fatlDir->Name1[i+1] == (uint8_t)0xFF) goto read_long_end;
		*name++ = fatlDir->Name1[i];
	}
	for (int i = 0; i < 12; i += 2) {
		if (fatlDir->Name2[i] == (uint8_t)0xFF && fatlDir->Name2[i+1] == (uint8_t)0xFF) goto read_long_end;
		*name++ = fatlDir->Name2[i];
	}
	for (int i = 0; i < 4; i += 2) {
		if (fatlDir->Name3[i] == (uint8_t)0xFF && fatlDir->Name3[i+1] == (uint8_t)0xFF) goto read_long_end;
		*name++ = fatlDir->Name3[i];
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
int get_full_name(struct FatShortDir *dir, unsigned char *buf, struct FatShortDir **nxtdir) {
	if (dir->Name[0] == (unsigned char)FAT_DIR_ENTRY_FREE) {
		return -E_FAT_READ_FREE_DIR;
	}
	if ((dir->Attr & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME) {
		if ((dir->Name[0] & FAT_LAST_LONG_ENTRY) != FAT_LAST_LONG_ENTRY) {
			// input long name entry isn't the first one
			return -E_FAT_BAD_DIR;
		}
		*buf = '\0';
		struct FatLongDir *ldir;
		for (ldir = (struct FatLongDir *)dir; (ldir->Attr & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME; ldir++) {
			read_and_cat_long_file_name(ldir, buf);
		}
		*nxtdir = (struct FatShortDir *)ldir + 1;
	}
	else {
		unsigned char *tmpbuf_ptr = buf;
		for (int i = 0; i < 11; i++) {
			if (dir->Name[i] == '\0' || dir->Name[i] == ' ') continue;
			if (i == 8) *tmpbuf_ptr++ = '.';
			*tmpbuf_ptr++ = dir->Name[i];
		}
		*tmpbuf_ptr++ = '\0';
		*nxtdir = dir + 1;
	}
	return 0;
}

// Overview:
//  read a directory at one cluster "clus"
//  and set the names as all names in the directory
//  where names are separated by '\0' and double '\0' indicates end
//  we assume that names is large enough to hold all names
//  when clus == 0, this function will read root dir
int read_dir(u_int clus, unsigned char *names, struct FatShortDir *dirs) {
	unsigned char buf[FAT_MAX_FILE_SIZE];
	try(read_fat_clusters(clus, buf, FAT_MAX_FILE_SIZE));
	unsigned char *buf_ptr = buf, *name_ptr = names;
	struct FatShortDir *fatDir = (struct FatShortDir *)buf_ptr;
	while (fatDir->Name[0]) {
		// empty file
		if (fatDir->Name[0] == FAT_DIR_ENTRY_FREE) {
			buf_ptr += sizeof(struct FatShortDir);
			fatDir = (struct FatShortDir *)buf_ptr;
			continue; 
		}
		try(get_full_name(fatDir, name_ptr, (struct FatShortDir **)&buf_ptr));
		if (dirs) {
			*dirs = *((struct FatShortDir *)buf_ptr - 1);
			dirs++;
		}
		fatDir = (struct FatShortDir *)buf_ptr;
		while (*name_ptr != '\0')name_ptr++;
		name_ptr++;
	}
	*name_ptr++ = '\0';
	// below is for debug
	unsigned char *tmp_name_ptr = names;
	debugf("read dir names: ");
	while (*tmp_name_ptr != '\0' || *(tmp_name_ptr + 1) != '\0') {
		// while (tmp_name_ptr != name_ptr) {
		if (*tmp_name_ptr == '\0') debugf(" ");
		else debugf("%c", *tmp_name_ptr);
		tmp_name_ptr++;
	}
	debugf("\nsize = %u\n", name_ptr - names);
	// above is for debug
	return name_ptr - names;
}

void debug_list_dir_contents(unsigned char *names, struct FatShortDir *dirs) {
	debugf("%20s %10s %10s %8s %6s %8s\n", "NAME", "SIZE", "CR_DATE", "CR_TIME", "ATTR", "CLUSNUM");
	unsigned char buffer[100];
	while (*names != '\0' || *(names + 1) != '\0') {
		while (*names == '\0') names++;
		unsigned char *bufp = buffer;
		while (*names != '\0') {
			*bufp++ = *names++;
		}
		*bufp = '\0';
		debugf("%20s ", buffer);
		if ((dirs->Attr & FAT_ATTR_DIRECTORY) == FAT_ATTR_DIRECTORY) {
			debugf("%10s ", "   <dir>");
		}
		else {
			debugf("%10u ", dirs->FileSize);
		}
		debug_print_date(dirs->CrtDate);
		debugf(" ");
		debug_print_time(dirs->CrtTime);
		debugf(" 0x%04X %8u\n", dirs->Attr, dirs->FstClusLO);

		dirs++;
	}
}

int find_dir(unsigned char *file_name, unsigned char *buf_ptr, struct FatShortDir **dir) {
	unsigned char name[BY2SECT];
	struct FatShortDir *fatDir = (struct FatShortDir *)buf_ptr;
	*dir = NULL;
	while (fatDir->Name[0]) {
		// empty file
		if (fatDir->Name[0] == (unsigned char)FAT_DIR_ENTRY_FREE) {
			buf_ptr += sizeof(struct FatShortDir);
			fatDir = (struct FatShortDir *)buf_ptr;
			continue; 
		}
		try(get_full_name(fatDir, name, (struct FatShortDir **)&buf_ptr));
		if (strcmp((char *)name, (char *)file_name) == 0) {
			*dir = fatDir;
			return 0;
		}
		fatDir = (struct FatShortDir *)buf_ptr;
	}
	return 0;
}

int free_dir(struct FatShortDir *pdir, unsigned char *file_name) {
	uint32_t clus = pdir->FstClusLO;
	unsigned char buf[FAT_MAX_FILE_SIZE], name[BY2SECT];
	try(read_fat_clusters(clus, buf, FAT_MAX_FILE_SIZE));
	struct FatShortDir *dir = NULL;
	try(find_dir(file_name, buf, &dir));
	if (!dir) {
		return -E_FAT_NOT_FOUND;
	}

	debugf("Found dir name = %s\nDoing Free.\n", name);

	while ((dir->Attr & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME) {
		dir->Name[0] = FAT_DIR_ENTRY_FREE;
		dir++;
	}
	dir->Name[0] = FAT_DIR_ENTRY_FREE;
	try(write_fat_clusters(clus, buf, FAT_MAX_FILE_SIZE));
	try(free_fat_clusters(dir->FstClusLO));
	return 0;
}

int encode_long_name(unsigned char *file_name, struct FatLongDir *ldirs) {
	int is_writing_name = 1;
	struct FatLongDir *stldirs = ldirs;
	for (int n = 0; n < FAT_MAX_ENT_NUM; n++, ldirs++) {
		if (!is_writing_name) {
			break;
		}
		for (int i = 0; i < 10; i += 2) {
			if (!is_writing_name) {
				ldirs->Name1[i] = 0xFF;
				ldirs->Name1[i+1] = 0xFF;
			} else if (*file_name == '\0') {
				is_writing_name = 0;
				ldirs->Name1[i] = 0;
				ldirs->Name1[i+1] = 0;
			} else {
				ldirs->Name1[i] = *file_name++;
				ldirs->Name1[i+1] = 0;
			}
		}
		for (int i = 0; i < 12; i += 2) {
			if (!is_writing_name) {
				ldirs->Name2[i] = 0xFF;
				ldirs->Name2[i+1] = 0xFF;
			} else if (*file_name == '\0') {
				is_writing_name = 0;
				ldirs->Name2[i] = 0;
				ldirs->Name2[i+1] = 0;
			} else {
				ldirs->Name2[i] = *file_name++;
				ldirs->Name2[i+1] = 0;
			}
		}
		for (int i = 0; i < 4; i += 2) {
			if (!is_writing_name) {
				ldirs->Name3[i] = 0xFF;
				ldirs->Name3[i+1] = 0xFF;
			} else if (*file_name == '\0') {
				is_writing_name = 0;
				ldirs->Name3[i] = 0;
				ldirs->Name3[i+1] = 0;
			} else {
				ldirs->Name3[i] = *file_name++;
				ldirs->Name3[i+1] = 0;
			}
		}
	}
	return ldirs - stldirs;
}

unsigned char encode_char(unsigned char ch) {
	if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z')) return ch;
	if (ch >= 'a' && ch <= 'z') return ch - 'a' + 'A';
	// $%'-_@~`!(){}^#&
	if (ch == '$' || ch == '%' || ch == '\'' || ch == '-' || ch == '@' || ch == '~' || ch == '`' ||
			ch == '!' || ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '^' || ch == '#' ||
			ch == '&') return ch;
	return '_';
}

int encode_short_name(struct FatShortDir *sdir, unsigned char *file_name, unsigned char pad_char) {
	u_int name_len = 0, ext_len = 0;
	unsigned char *fp = file_name;
	while (*fp != '.' && *fp != '\0') fp++;
	name_len = fp - file_name;
	if (*fp == '.') {
		while (*fp != '\0') fp++;
		ext_len = (fp - file_name) - name_len - 1;
	}
	for (int i = 0; i < 11; i++) sdir->Name[i] = 0;
	if (name_len > 8 || ext_len > 3) {
		name_len =  name_len > 6 ? 6 : name_len;
		ext_len = ext_len > 3 ? 3 : ext_len;
		sdir->Name[name_len] = '~';
		sdir->Name[name_len+1] = pad_char;
	}
	for (int i = 0; i < 8; i++) {
		if (!sdir->Name[i]) sdir->Name[i] = i < name_len ? encode_char(file_name[i]) : ' ';
	}
	for (int i = 0; i < 3; i++) {
		if (!sdir->Name[i+8]) sdir->Name[i+8] = i < ext_len ? encode_char(file_name[name_len + 1 + i]) : ' ';
	}
	return 0;
}

int write_dir_entry(struct FatShortDir *buf_ptr, struct FatShortDir *sdir, struct FatLongDir *ldirs, uint32_t ldir_cnt) {
	struct FatLongDir *target = (struct FatLongDir *)buf_ptr;
	for (int i = 0; i < ldir_cnt + 1; i++) {
		if (i < ldir_cnt) {
			target[i] = ldirs[ldir_cnt - i - 1];
		}
		else {
			*((struct FatShortDir *)(&target[i])) = *sdir;
		}
	}
	return 0;
}

int alloc_and_write_dir_entry(unsigned char *buf_ptr, struct FatShortDir *sdir, struct FatLongDir *ldirs, uint32_t ldir_cnt, uint32_t pdir_max_entry) {
	struct FatShortDir *fatDir = (struct FatShortDir *) buf_ptr, *st_ptr = NULL;
	uint32_t cnt = 0, total_entry = 0;
	while (fatDir->Name[0]) {
		total_entry++;
		if (total_entry > pdir_max_entry) {
			return -E_FAT_DIR_FULL;
		}
		if (fatDir->Name[0] == (unsigned char)FAT_DIR_ENTRY_FREE) {
			if (cnt == 0) {
				st_ptr = fatDir;
			}
			cnt++;
			if (cnt >= ldir_cnt) {
				try(write_dir_entry(st_ptr, sdir, ldirs, ldir_cnt));
				return 0;
			}
		}
		else {
			st_ptr = NULL;
			cnt = 0;
		}
		buf_ptr += sizeof(struct FatShortDir);
		fatDir = (struct FatShortDir *)buf_ptr;
	}
	if (cnt) {
		if (total_entry + ldir_cnt + 1 - cnt > pdir_max_entry) {
			return -E_FAT_DIR_FULL;
		}
		try(write_dir_entry(st_ptr, sdir, ldirs, ldir_cnt));
		return 0;
	}
	if (total_entry + ldir_cnt + 1 > pdir_max_entry) {
		return -E_FAT_DIR_FULL;
	}
	try(write_dir_entry(fatDir, sdir, ldirs, ldir_cnt));
	return 0;
}

int create_file(struct FatShortDir *pdir, unsigned char *file_name, unsigned char *file_content, uint32_t size, unsigned char Attr) {
	if ((pdir->Attr & FAT_ATTR_DIRECTORY) != FAT_ATTR_DIRECTORY || (pdir->Attr & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME) {
		return -E_FAT_BAD_DIR;
	}
	uint32_t clus = pdir->FstClusLO;
	if (Attr & ~(FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM)) {
		return -E_FAT_BAD_ATTR;
	}
	uint32_t clus_cnt = (size + fatBPB.BytsPerSec * fatBPB.SecPerClus - 1) / (fatBPB.BytsPerSec * fatBPB.SecPerClus);
	uint32_t name_len = strlen((char *)file_name);
	if (name_len / FAT_LONG_NAME_LEN > FAT_MAX_ENT_NUM) {
		return -E_FAT_NAME_TOO_LONG;
	}

	unsigned char buf[FAT_MAX_FILE_SIZE];
	try(read_fat_clusters(clus, buf, FAT_MAX_FILE_SIZE));

	struct FatShortDir *tmpdirp;
	try(find_dir(file_name, buf, &tmpdirp));
	if (tmpdirp != NULL) {
		return -E_FAT_NAME_DUPLICATED;
	}

	uint32_t pdir_size;
	try(query_fat_clusters(clus, &pdir_size));

	struct FatShortDir sdir;
	unsigned char pad_char = '1';
	// unsigned char name_buf[20];
	/*
	// useless code due to find_dir won't compare short name in long ones
	do {
		try(encode_short_name(&sdir, file_name, pad_char));
		try(get_full_name(&sdir, name_buf, &tmp_dir_p));
		try(find_dir(name_buf, buf, &tmp_dir_p));
		pad_char++;
		if (pad_char == '6') pad_char = 'A';
	} while (tmp_dir_p != NULL);*/

	try(encode_short_name(&sdir, file_name, pad_char));

	sdir.Attr = Attr;
	sdir.NTRes = 0;

	uint32_t year, month, date, hour, minute, second, timestamp, timeus;
	timestamp = get_time(&timeus);
	try(get_all_time(timestamp, &year, &month, &date, &hour, &minute, &second));
	hour += 8; // This is UTC, we need CST
	get_fat_time(year, month, date, hour, minute, second, timeus, &sdir.CrtTimeTenth, &sdir.CrtTime, &sdir.CrtDate);
	sdir.LstAccDate = sdir.CrtDate;
	sdir.FstClusHI = 0;
	sdir.WrtTime = sdir.CrtTime;
	sdir.WrtDate = sdir.CrtDate;
	uint32_t alloc_clus;
	try(alloc_fat_clusters(&alloc_clus, clus_cnt));
	sdir.FstClusLO = (uint16_t)alloc_clus;
	sdir.FileSize = size;
	struct FatLongDir ldirs[FAT_MAX_ENT_NUM];
	uint32_t ldir_cnt = encode_long_name(file_name, ldirs);
	uint8_t check_sum = generate_long_file_check_sum(sdir.Name);
	for (int i = 0; i < ldir_cnt; i++) {
		ldirs[i].Attr = FAT_ATTR_LONG_NAME;
		ldirs[i].Chksum = check_sum;
		ldirs[i].FstClusLO = 0;
		ldirs[i].Type = 0;
		ldirs[i].Ord = i == ldir_cnt - 1 ? ((i + 1) | FAT_LAST_LONG_ENTRY) : i + 1;
	}
	try(alloc_and_write_dir_entry(buf, &sdir, ldirs, ldir_cnt, pdir_size * fatBPB.BytsPerSec * fatBPB.SecPerClus / sizeof(struct FatShortDir)));
	try(write_fat_clusters(clus, buf, FAT_MAX_FILE_SIZE));
	try(write_fat_clusters(alloc_clus, file_content, size));
	return 0;
}
