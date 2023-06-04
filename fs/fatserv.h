#include <fatfs.h>
#include <lib.h>
#include <mmu.h>

#define PTE_DIRTY 0x0002 // file system block cache is dirty

/* IDE disk number to look on for our file system */
#define DISKNO 1

/* Disk block n, when in memory, is mapped into the file system
 * server's address space at DISKMAP+(n*BY2BLK). */
#define FATVAMIN 0x10000000
#define FATVAMAX 0x50000000
#define FATVARANGE FATVAMAX - FATVAMIN

#define FAT_MAX_CLUS_SIZE 8192
#define FAT_MAX_SPACE_SIZE FAT_MAX_CLUS_SIZE

/* ide.c */
extern void ide_read(u_int diskno, u_int secno, void *dst, u_int nsecs);
extern void ide_write(u_int diskno, u_int secno, void *src, u_int nsecs);

/* fatfs.c */
int fat_file_open(char *path, struct File **pfile);
int fat_file_get_block(struct File *f, u_int blockno, void **pblk);
int fat_file_set_size(struct File *f, u_int newsize);
void fat_file_close(struct File *f);
int fat_file_remove(char *path);
int fat_file_dirty(struct File *f, u_int offset);
void fat_file_flush(struct File *);

void fat_fs_init(void);
void fat_fs_sync(void);
int fat_map_block(u_int);
int fat_alloc_block(void);
