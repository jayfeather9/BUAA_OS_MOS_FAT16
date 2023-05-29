#include <fs.h>
#include <lib.h>
#include "../../fs/fat.h"

struct FatBPB usrfatBPB;
struct FatDisk usrfatDisk;

static int fat_close(struct Fd *fd);
static int fat_read(struct Fd *fd, void *buf, u_int n, u_int offset);
static int fat_write(struct Fd *fd, const void *buf, u_int n, u_int offset);
static int fat_stat(struct Fd *fd, struct Stat *stat);

struct Dev devfat = {
    .dev_id = 'a',
    .dev_name = "fat",
    .dev_read = fat_read,
    .dev_write = fat_write,
    .dev_close = fat_close,
    .dev_stat = fat_stat,
};

int fat_close(struct Fd *fd) {
	return 0;
}

int fat_read(struct Fd *fd, void *buf, u_int n, u_int offset) {
	return 0;
}

int fat_write(struct Fd *fd, const void *buf, u_int n, u_int offset) {
	return 0;
}

int fat_stat(struct Fd *fd, struct Stat *stat) {
	return 0;
}

// unsigned char initbuf[BY2PG];
int fat_user_init() {
	void *initbuf = (void *)FATINFOBASE;
	fatipc_fatinit((struct FatBPB *)initbuf);
	// debugf("BPB val %u\n", (*(char *)initbuf));
	usrfatBPB = *(struct FatBPB *)initbuf;
	usrfatDisk = *(struct FatDisk *)(initbuf + sizeof(struct FatBPB));
	// debugf("BPB val %u\n", usrfatBPB.BytsPerSec);
	// debugf("%u %u %u\n", usrfatDisk.CountofClusters, usrfatDisk.FirstRootDirSecNum, usrfatDisk.FATSz);
	return 0;
}