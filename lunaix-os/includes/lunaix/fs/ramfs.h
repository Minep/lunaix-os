#ifndef __LUNAIX_RAMFS_H
#define __LUNAIX_RAMFS_H

#include <lunaix/types.h>

#define RAMF_FILE 0
#define RAMF_DIR 1
#define RAMF_SYMLINK 2

struct ram_inode
{
    u32_t flags;
    char* symlink;
};

#define RAM_INODE(data) ((struct ram_inode*)(data))

void
ramfs_init();

#endif /* __LUNAIX_RAMFS_H */
