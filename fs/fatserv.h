#ifndef _FATSERV_H_
#define _FATSERV_H_

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
#define FATVARANGE (FATVAMAX - FATVAMIN)
#define FATROOTVA (FATVAMAX + 0x1000)

#define FAT_MAX_CLUS_SIZE 8192
#define FAT_MAX_SPACE_SIZE FAT_MAX_CLUS_SIZE

/* ide.c */
extern void ide_read(u_int diskno, u_int secno, void *dst, u_int nsecs);
extern void ide_write(u_int diskno, u_int secno, void *src, u_int nsecs);

/* fatfs.c */
// int fat_file_open(char *path, struct File **pfile);
// int fat_file_get_block(struct File *f, u_int blockno, void **pblk);
// int fat_file_set_size(struct File *f, u_int newsize);
// void fat_file_close(struct File *f);
// int fat_file_remove(char *path);
// int fat_file_dirty(struct File *f, u_int offset);
// void fat_file_flush(struct File *);

void fat_fs_init(void);
struct FATDIRENT *fat_get_root();
int read_disk_fat_cluster(uint32_t clus, unsigned char *buf);
int is_clus_mapped(uint32_t clus, uint32_t *va);
void debug_print_fatBPB();
void debug_print_fatDisk();
void debug_print_short_dir(struct FATDIRENT *dir, uint32_t num);
int debug_print_file_as_dir_entry(char *buf);
void debug_print_fatsec(uint32_t secno);
int get_fat_entry(uint32_t clus, uint32_t *pentry_val);
int fat_file_get_cluster_by_order(struct FATDIRENT *ent, u_int fileclno, uint32_t *pclus, u_int alloc);
int fat_file_clear_clus(struct FATDIRENT *ent, u_int fileclno);
int fat_file_get_clus(struct FATDIRENT *ent, u_int fileclno, void **data);
void fat_write_clus(u_int clus);
int fat_read_clus(u_int clus, void **pva, u_int *isnew);
int fat_dirty_clus(u_int clus);
int fat_dir_lookup(struct FATDIRENT *dir, char *name, struct FATDIRENT **ent);
int fat_dir_alloc_files(struct FATDIRENT *dir, struct FATDIRENT **file, u_int count);

// void fat_fs_sync(void);
// int fat_map_block(u_int);
// int fat_alloc_block(void);

#endif // _FATSERV_H_