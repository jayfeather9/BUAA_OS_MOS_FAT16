#ifndef _FAT_H_
#define _FAT_H_

#include <stdint.h>
#include <fs.h>
#include <lib.h>
#include <mmu.h>

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
	uint32_t DataSec;						// the count of sectors in the data region }

void fat_init();
void debug_print_fatBPB();
void debug_print_fatsec(uint32_t secno);

#endif
