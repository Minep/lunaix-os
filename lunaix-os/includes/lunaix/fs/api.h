#ifndef __LUNAIX_FSAPI_H
#define __LUNAIX_FSAPI_H

#include <lunaix/fs.h>
#include <lunaix/fcntl_defs.h>

struct fsapi_vsb_ops
{
    u32_t (*read_capacity)(struct v_superblock* vsb);
    u32_t (*read_usage)(struct v_superblock* vsb);
    void (*init_inode)(struct v_superblock* vsb, struct v_inode* inode);
};

static inline struct device* 
fsapi_blockdev(struct v_superblock* vsb) 
{
    assert_fs(vsb->dev);
    return vsb->dev;
}

typedef void (*inode_init)(struct v_superblock* vsb, struct v_inode* inode) ;

static inline void
fsapi_set_inode_initiator(struct v_superblock* vsb, inode_init inode_initiator)
{
    vsb->ops.init_inode = inode_initiator;
}

static inline void
fsapi_set_block_size(struct v_superblock* vsb, size_t block_size)
{
    vsb->blksize = block_size;
}

static inline size_t
fsapi_block_size(struct v_superblock* vsb)
{
    return vsb->blksize;
}

static inline void
fsapi_set_vsb_ops(struct v_superblock* vsb, struct fsapi_vsb_ops* basic_ops)
{
    vsb->ops.read_capacity = basic_ops->read_capacity;
    vsb->ops.read_usage = basic_ops->read_usage;
}

static inline void
fsapi_complete_vsb(struct v_superblock* vsb, void* cfs_sb)
{
    assert_fs(vsb->ops.init_inode);
    assert_fs(vsb->ops.read_capacity);
    assert_fs(vsb->blksize);

    vsb->data = cfs_sb;
}

static inline bool
fsapi_readonly_mount(struct v_superblock* vsb)
{
    return (vsb->root->mnt->flags & MNT_RO);
}

static inline void
fsapi_set_readonly_mount(struct v_superblock* vsb)
{
    vsb->root->mnt->flags |= MNT_RO;
}

#define fsapi_impl_data(vfs_obj, type) (type*)((vfs_obj)->data)

#endif /* __LUNAIX_FSAPI_H */
