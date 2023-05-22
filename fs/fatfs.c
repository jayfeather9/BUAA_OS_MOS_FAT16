#include <lib.h>

#include "serv.h"
#include "fat.h"

#define DISKNO 1

struct FatBPB fatBPB;

char fat_buf[BY2SECT];

void read_array(unsigned char **buf, int len, unsigned char *ret) {
    for (int i = 0; i < len; i++) {
        ret[i] = **buf;
        (*buf)++;
    }
}

void read_little_endian(unsigned char **buf, int len, uint32_t *ret) {
    *ret = 0;
    for (int i = len - 1; i >= 0; i--) {
        *ret = *ret * 256 + (*buf)[i];
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
    debugf("VolID: %d\n", fatBPB.VolID);
    memset(tmp_buf, 0, 32);
    memcpy(tmp_buf, fatBPB.VolLab, 11);
    debugf("VolLab: %s\n", tmp_buf);
    memset(tmp_buf, 0, 32);
    memcpy(tmp_buf, fatBPB.FilSysType, 8);
    debugf("FilSysType: %s\n", tmp_buf);
    debugf("Signature_word: 0x%02X 0x%02X\n", fatBPB.Signature_word[0], fatBPB.Signature_word[1]);
    debugf("====== end of fat BPB ======\n");
}

void debug_print_fatsec(uint32_t secno) {
    unsigned char buf[1024];
	ide_read(1, secno, buf, 1);
    debugf("========= printing fat section %u =========\n", secno);
	for (int i = 0; i < 32; i++) {
		debugf("0x%4x-0x%4x: ", i*8*4, (i*8+7)*4+3);
		for (int j = 0; j < 32; j++) {
			debugf("%02X ", buf[i*32+j]);
		}
		debugf("\n");
	}
    debugf("========= end of fat section %u ===========\n", secno);
}