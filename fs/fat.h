#ifndef _FAT_H_
#define _FAT_H_

#include <stdint.h>
#include <fs.h>
#include <lib.h>
#include <mmu.h>

#define E_FAT_BAD_CLUSTER 0x1000
#define E_FAT_CLUSTER_FULL 0x1001

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
};

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE 0x20
#define FAT_ATTR_LONG_NAME 0x0F 
// defined as (READ_ONLY | HIDDEN | SYSTEM | VOLUME_ID)
// upper 2 bits of attr are reserved and should be 0

struct FatDir {
	unsigned char Name[11];
	uint32_t Attr;
	uint32_t CrtTimeTenth;
	uint32_t CrtTime;
	uint32_t CrtDate;
	uint32_t LstAccDate;
	uint32_t WrtTime;
	uint32_t WrtDate;
	uint32_t FstClusLO;
	uint32_t FileSize;
};
// uint32_t NTRes; // reserved and should be 0
// uint32_t FstClusHI; // for FAT16, must be 0

void fat_init();
void debug_print_fatBPB();
void debug_print_fatsec(uint32_t secno);
void debug_print_fatDisk();
int get_fat_entry(uint32_t clus, uint32_t *pentry_val);
int set_fat_entry(uint32_t clus, uint32_t entry_val);
void debug_print_fat_entry(uint32_t clus);
int read_fat_cluster(uint32_t clus, unsigned char *buf);
int write_fat_cluster(uint32_t clus, unsigned char *buf);
int alloc_fat_clusters(uint32_t *pclus, uint32_t count);
int expand_fat_clusters(uint32_t *pclus, uint32_t count);
int free_fat_clusters(uint32_t clus);
int get_cluster_data(uint32_t clus, unsigned char *buf);
int set_cluster_data(uint32_t clus, unsigned char *buf);
void debug_print_cluster_data(uint32_t clus);

#endif

