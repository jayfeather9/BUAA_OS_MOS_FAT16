#include <fs.h>
#include <lib.h>
#include "../../fs/fat.h"

static int fat_close(struct Fd *fd);
static int fat_read(struct Fd *fd, void *buf, u_int n, u_int offset);
static int fat_write(struct Fd *fd, const void *buf, u_int n, u_int offset);
static int fat_stat(struct Fd *fd, struct Stat *stat);

struct Dev devfat = {
    .dev_id = 'f',
    .dev_name = "file",
    .dev_read = fat_read,
    .dev_write = fat_write,
    .dev_close = fat_close,
    .dev_stat = fat_stat,
};