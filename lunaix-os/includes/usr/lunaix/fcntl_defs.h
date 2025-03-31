#ifndef __LUNAIX_SYS_FCNTL_DEFS_H
#define __LUNAIX_SYS_FCNTL_DEFS_H

#include "fstypes.h"
#include "types.h"

#define FO_CREATE   0x1
#define FO_APPEND   0x2
#define FO_DIRECT   0x4
#define FO_WRONLY   0x8
#define FO_RDONLY   0x10
#define FO_RDWR     0x20
#define FO_TRUNC    0x40

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
#define O_TRUNC FO_TRUNC

#define AT_SYMLINK_FOLLOW       0b0000
#define AT_SYMLINK_NOFOLLOW     0b0001
#define AT_FDCWD                0b0010
#define AT_EACCESS              0b0100

#define R_OK                    0b100100100
#define W_OK                    0b010010010
#define X_OK                    0b001001001
#define F_OK                    0b111111111

/* Mount with read-only flag */
#define MNT_RO (1 << 0)

/* Mount with block-cache-disabled flag */
#define MNT_NC (1 << 1)

typedef unsigned int mode_t;
typedef unsigned int nlink_t;

struct file_stat
{
    dev_t   st_dev;
    ino_t   st_ino;
    mode_t  st_mode;
    nlink_t st_nlink;
    uid_t   st_uid;
    gid_t   st_gid;
    dev_t   st_rdev;
    off_t   st_size;
    size_t  st_blksize;
    size_t  st_blocks;

    unsigned long st_atim;
    unsigned long st_ctim;
    unsigned long st_mtim;

    size_t  st_ioblksize;
};

#endif /* __LUNAIX_FNCTL_DEFS_H */
