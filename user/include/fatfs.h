#ifndef _FS_H_
#define _FS_H_ 1

#include <stdint.h>

// Maximum size of a filename (a single path component), including null
#define MAXNAMELEN 128

// Maximum size of a complete pathname, including null
#define MAXPATHLEN 1024

#define BY2DIRENT 32
#define BY2FILE 256

// File types
#define FTYPE_REG 0 // Regular file
#define FTYPE_DIR 1 // Directory

// File system super-block (both in-memory and on-disk)

#define FS_MAGIC 0x68286097 // Everyone's favorite OS class

struct FATBPB {
	char BS_jmpBoot[3];
	char BS_OEMName[8];
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
	char BS_VolLab[11];
	char BS_FilSysType[8];
};

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
	char DIR_Name[11];
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
};

struct FATLONGNAME {
	uint8_t LDIR_Ord;
	uint16_t LDIR_Name1[5];
	uint8_t LDIR_Attr;
	uint8_t LDIR_Type;
	uint8_t LDIR_Chksum;
	uint16_t LDIR_Name2[6];
	uint16_t LDIR_FstClusLO;
	uint16_t LDIR_Name3[2];
};

struct FatFile {
	char f_name[MAXNAMELEN]; // filename
	struct FATDIRENT dir_ent; // dir entry
	struct FatFile *f_dir;
	char f_pad[BY2FILE - MAXNAMELEN - BY2DIRENT - sizeof(void *)];
} __attribute__((aligned(4), packed));

struct FatSuper {
	uint32_t s_magic;   // Magic number: FS_MAGIC
	uint32_t s_nblocks; // Total number of blocks on disk
	struct FatFile s_root; // Root directory node
};

struct FatSpace {
	uint32_t st_va;
	uint32_t size;
	uint32_t clus;
	struct FatSpace *nxt, *prev;
};

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

#endif // _FS_H_
