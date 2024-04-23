#ifndef __LUNAIX_FS_EXT2_UTILS_H
#define __LUNAIX_FS_EXT2_UTILS_H

#include <lunaix/fs/api.h>

#include "ext2.h"

static inline int
__read_block(struct ext2_sbinfo* vsb, void* buf, size_t blk_id, size_t cnt)
{
    struct device* bdev = vsb->bdev;
    size_t blksz = vsb->block_size;
    size_t off   = blk_id * blksz;
    size_t sz    = cnt * blksz;
    return bdev->ops.read(bdev, buf, off, sz);
}

static inline int
__read_block_unaligned(struct ext2_sbinfo* vsb, void* buf, size_t blk_id, size_t size)
{
    struct device* bdev = vsb->bdev;
    size_t blksz = vsb->block_size;
    size_t off   = blk_id * blksz;
    return bdev->ops.read(bdev, buf, off, size);
}

void*
__alloc_block_buffer(struct v_superblock* vsb);

void
__free_block_buffer(struct v_superblock* vsb, void* buf);

#endif /* __LUNAIX_FS_EXT2_UTILS_H */
