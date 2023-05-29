#ifndef _FAT_H_
#define _FAT_H_

// set as double root size for buffer size
// not FAT max size, but this file system max size
#define FAT_MAX_FILE_SIZE 32768
// similar as above, set as SecPerClus=16
#define FAT_MAX_CLUS_SIZE 8192
#define FAT_MAX_SPACE_SIZE FAT_MAX_CLUS_SIZE
#define FAT_MAX_ENT_NUM 16
#define FAT_MAX_NAME_LEN 512

#define FAT_LONG_NAME_LEN 13

#define E_FAT_BAD_CLUSTER 0x1000
#define E_FAT_CLUSTER_FULL 0x1001
#define E_FAT_DIFF 0x1002
#define E_FAT_ACCESS_FREE_CLUSTER 0x1003
#define E_FAT_BAD_ATTR 0x1004
#define E_FAT_BAD_DIR 0x1005
#define E_FAT_READ_FREE_DIR 0x1006
#define E_FAT_NOT_FOUND 0x1007
#define E_FAT_NAME_TOO_LONG 0x1008
#define E_FAT_DIR_FULL 0x1009
#define E_FAT_NAME_DUPLICATED 0x100A
#define E_FAT_MAX_OPEN 0x100B
#define E_FAT_INVAL 0x100C
#define E_FAT_VA_FULL 0x100D


struct FatBPB {
	unsigned char jmpBoot[3];
	unsigned char OEMName[8];
	uint32_t BytsPerSec;
	uint32_t SecPerClus;
	uint32_t RsvdSecCnt;
	uint32_t NumFATs;
	uint32_t RootEntCnt;
	uint32_t TotSec16;
	uint32_t Media;
	uint32_t FATSz16;
	uint32_t SecPerTrk;
	uint32_t NumHeads;
	uint32_t HiddSec;
	uint32_t TotSec32;
	uint32_t DrvNum;
	uint32_t Reserved1;
	uint32_t BootSig;
	uint32_t VolID;
	unsigned char VolLab[11];
	unsigned char FilSysType[8];
	unsigned char Signature_word[2];
};

struct FatDisk {
	uint32_t RootDirSectors;		// the count of sectors occupied by the root directory
	uint32_t FATSz;							// FAT size
	uint32_t TotSec;						// Total section count
	uint32_t DataSec;						// the count of sectors in the data region
	uint32_t CountofClusters;		// the count of clusters
	uint32_t FirstRootDirSecNum;
};

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE 0x20
#define FAT_ATTR_LONG_NAME 0x0F
// defined as (READ_ONLY | HIDDEN | SYSTEM | VOLUME_ID)
#define FAT_ATTR_LONG_NAME_MASK 0x3F
// defined as (READ_ONLY | HIDDEN | SYSTEM | VOLUME_ID | DIRECTORY | ARCHIVE)

// upper 2 bits of attr are reserved and should be 0

#define FAT_LAST_LONG_ENTRY 0x40

#define FAT_DIR_ENTRY_FREE 0xE5

struct FatShortDir {
	unsigned char Name[11];
	uint8_t Attr;
	uint8_t NTRes;
	uint8_t CrtTimeTenth;
	uint16_t CrtTime;
	uint16_t CrtDate;
	uint16_t LstAccDate;
	uint16_t FstClusHI;
	uint16_t WrtTime;
	uint16_t WrtDate;
	uint16_t FstClusLO;
	uint32_t FileSize;
};

struct FatLongDir {
	uint8_t Ord;
	uint8_t Name1[10];
	uint8_t Attr;
	uint8_t Type;
	uint8_t Chksum;
	uint8_t Name2[12];
	uint16_t FstClusLO;
	uint8_t Name3[4];
};

#define FAT_MIN_VA 0x10000000
#define FAT_MAX_VA 0x50000000
#define FAT_VA_LEN FAT_MAX_VA - FAT_MIN_VA

struct FatSpace {
	uint32_t st_va;
	uint32_t size;
	uint32_t clus;
	struct FatSpace *nxt, *prev;
};

struct FatBPB *get_fat_BPB();
struct FatDisk *get_fat_disk();

void debug_print_fspace();
int insert_space(uint32_t st_va, uint32_t size);
int alloc_fat_file_space(uint32_t clus, uint32_t bysize, uint32_t *va);
int free_clus(uint32_t clus);

void fat_init();
void debug_print_fatBPB();
void debug_print_fatsec(uint32_t secno);
void debug_print_fatDisk();
unsigned char encode_char(unsigned char ch);
int get_fat_entry(uint32_t clus, uint32_t *pentry_val);
int set_fat_entry(uint32_t clus, uint32_t entry_val);
void debug_print_fat_entry(uint32_t clus);
int read_fat_cluster(uint32_t clus, unsigned char *buf, uint32_t nbyts);
int write_fat_cluster(uint32_t clus, unsigned char *buf, uint32_t nbyts);
int read_fat_clusters(uint32_t clus, unsigned char *buf, uint32_t nbyts);
int write_fat_clusters(uint32_t clus, unsigned char *buf, uint32_t nbyts);
void debug_print_cluster_data(uint32_t clus);
int query_fat_clusters(uint32_t st_clus, uint32_t *size);
int alloc_fat_clusters(uint32_t *pclus, uint32_t count);
int expand_fat_clusters(uint32_t *pclus, uint32_t count);
int free_fat_clusters(uint32_t clus);
void debug_print_short_dir(struct FatShortDir *dir);
void debug_print_long_dir(struct FatLongDir *dir);
int debug_print_file_as_dir_entry(uint32_t clus);
unsigned char generate_long_file_check_sum(unsigned char *pFcbName);
int get_full_name(struct FatShortDir *dir, unsigned char *buf, struct FatShortDir **nxtdir);
int read_dir(u_int clus, unsigned char *names, struct FatShortDir *dirs);
void debug_list_dir_contents(unsigned char *names, struct FatShortDir *dirs);
int free_dir(struct FatShortDir *pdir, unsigned char *file_name);
int create_file(struct FatShortDir *pdir, unsigned char *file_name, unsigned char *buf, uint32_t size, unsigned char Attr);
int read_file(struct FatShortDir *pdir, unsigned char *file_name, unsigned char *buf, uint32_t read_len);
int write_file(struct FatShortDir *pdir, unsigned char *file_name, unsigned char *buf, uint32_t write_len);
int walk_path_fat(unsigned char *path, struct FatShortDir *pdir, struct FatShortDir *pfile);
int fat_map_file(struct FatShortDir *f, uint32_t *va, uint32_t alloc);
int fat_get_va(struct FatShortDir *f, uint32_t pageno, uint32_t *va);

#endif

