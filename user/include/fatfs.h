#ifndef _FATFS_H_
#define _FATFS_H_ 1

#include <stdint.h>

// Maximum size of a filename (a single path component), including null
#define MAXNAMELEN 128

// Maximum size of a complete pathname, including null
#define MAXPATHLEN 1024

#define BY2DIRENT 32
#define BY2FILE 256

#define FAT_MAX_CLUS_SIZE 8192
#define FAT_MAX_SPACE_SIZE FAT_MAX_CLUS_SIZE
#define FAT_MAX_ROOT_SEC_NUM 128
#define FAT_MAX_ROOT_BYTES 65536
#define FAT_MAX_ENT_NUM 63

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

#define FAT_LONG_NAME_CHAR_CNT 13

// File system super-block (both in-memory and on-disk)

#define FS_MAGIC 0x68286097 // Everyone's favorite OS class

struct FATBPB {
	unsigned char BS_jmpBoot[3];
	unsigned char BS_OEMName[8];
	uint16_t BPB_BytsPerSec;
	uint8_t BPB_SecPerClus;
	uint16_t BPB_RsvdSecCnt;
	uint8_t BPB_NumFATs;
	uint16_t BPB_RootEntCnt;
	uint16_t BPB_TotSec16;
	uint8_t BPB_Media;
	uint16_t BPB_FATSz16;
	uint16_t BPB_SecPerTrk;
	uint16_t BPB_NumHeads;
	uint32_t BPB_HiddSec;
	uint32_t BPB_TotSec32;
	uint8_t BS_DrvNum;
	uint8_t BS_Reserved1;
	uint8_t BS_BootSig;
	uint32_t BS_VolID;
	uint8_t BS_VolLab[11];
	uint8_t BS_FilSysType[8];
}__attribute__((aligned(4), packed));

struct FATDISK {
	uint32_t RootDirSectors;		// the count of sectors occupied by the root directory
	uint32_t FATSz;							// FAT size
	uint32_t TotSec;						// Total section count
	uint32_t DataSec;						// the count of sectors in the data region
	uint32_t CountofClusters;		// the count of clusters
	uint32_t FirstRootDirSecNum;// the first sector number of the root directory
	uint32_t BytsPerClus;				// the count of bytes in a cluster
};

struct FATDIRENT {
	uint8_t DIR_Name[11];
	uint8_t DIR_Attr;
	uint8_t DIR_NTRes;
	uint8_t DIR_CrtTimeTenth;
	uint16_t DIR_CrtTime;
	uint16_t DIR_CrtDate;
	uint16_t DIR_LstAccDate;
	uint16_t DIR_FstClusHI;
	uint16_t DIR_WrtTime;
	uint16_t DIR_WrtDate;
	uint16_t DIR_FstClusLO;
	uint32_t DIR_FileSize;
}__attribute__((aligned(4), packed));

struct FATLONGNAME {
	uint8_t LDIR_Ord;
	uint8_t LDIR_Name1[10];
	uint8_t LDIR_Attr;
	uint8_t LDIR_Type;
	uint8_t LDIR_Chksum;
	uint8_t LDIR_Name2[12];
	uint16_t LDIR_FstClusLO;
	uint8_t LDIR_Name3[4];
}__attribute__((aligned(4), packed));

struct FatFile {
	char f_name[MAXNAMELEN]; // filename
	struct FATDIRENT dir_ent; // dir entry
	struct FatFile *f_dir;
	char f_pad[BY2FILE - MAXNAMELEN - BY2DIRENT - sizeof(void *)];
};

struct FatSpace {
	uint32_t st_va;
	uint32_t size;
	uint32_t clus;
	struct FatSpace *nxt, *prev;
};

#define E_FAT_NOT_FOUND 1000
#define E_FAT_VA_FULL 1001
#define E_FAT_CLUS_UNMAPPED 1002
#define E_FAT_BAD_CLUSTER 1003
#define E_FAT_ENT_DIFF 1004
#define E_FAT_ACCESS_FREE_CLUS 1005
#define E_FAT_CLUSTER_FULL 1006
#define E_FAT_READ_FREE_DIR 1007
#define E_FAT_BAD_DIR 1008
#define E_FAT_BAD_PATH 1009
#define E_FAT_FILE_EXISTS 1010
#define E_FAT_INVAL 1011
#define E_FAT_SHRINK_ALL 1012
#define E_FAT_MAX_OPEN 1013

#endif // _FATFS_H_
