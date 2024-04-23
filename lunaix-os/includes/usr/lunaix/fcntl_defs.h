#ifndef __LUNAIX_SYS_FCNTL_DEFS_H
#define __LUNAIX_SYS_FCNTL_DEFS_H

#include "fstypes.h"
#include "types.h"

#define FO_CREATE 0x1
#define FO_APPEND 0x2
#define FO_DIRECT 0x4
#define FO_WRONLY 0x8
#define FO_RDONLY 0x10
#define FO_RDWR 0x20

#define FO_NOFOLLOW 0x10000

#define FSEEK_SET 0x1
#define FSEEK_CUR 0x2
#define FSEEK_END 0x3

#define O_CREAT FO_CREATE
#define O_APPEND FO_APPEND
#define O_DIRECT FO_DIRECT
#define O_WRONLY FO_WRONLY
#define O_RDONLY FO_RDONLY
#define O_RDWR FO_RDWR

/* Mount with read-only flag */
#define MNT_RO (1 << 0)

/* Mount with block-cache-disabled flag */
#define MNT_NC (1 << 1)

struct file_stat
{
    dev_t st_dev;
    ino_t st_ino;
    unsigned int mode;
    dev_t st_rdev;
    off_t st_size;
    size_t st_blksize;
    size_t st_ioblksize;
    size_t st_blocks;
};

#endif /* __LUNAIX_FNCTL_DEFS_H */
