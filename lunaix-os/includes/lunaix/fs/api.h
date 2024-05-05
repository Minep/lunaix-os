#ifndef __LUNAIX_FSAPI_H
#define __LUNAIX_FSAPI_H

#include <lunaix/fs.h>
#include <lunaix/fcntl_defs.h>
#include <lunaix/blkbuf.h>

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
typedef void (*inode_free)(struct v_inode* inode) ;

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
    vsb->blks = blkbuf_create(vsb->dev, vsb->blksize);
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

static inline void
fsapi_inode_setid(struct v_inode* inode, 
                  inode_t i_id, unsigned int blk_addr)
{
    inode->id = i_id;
    inode->lb_addr = blk_addr;
}

static inline void
fsapi_inode_settype(struct v_inode* inode, unsigned int type)
{
    inode->itype = type;
}

static inline void
fsapi_inode_setsize(struct v_inode* inode, unsigned int fsize)
{
    inode->lb_usage = ICEIL(fsize, inode->sb->blksize);
    inode->fsize = fsize;
}

static inline void
fsapi_inode_setops(struct v_inode* inode, 
                   struct v_inode_ops* ops)
{
    inode->ops = ops;
}

static inline void
fsapi_inode_setfops(struct v_inode* inode, 
                   struct v_file_ops* fops)
{
    inode->default_fops = fops;
}

static inline void
fsapi_inode_setdector(struct v_inode* inode, 
                      inode_free free_cb)
{
    inode->destruct = free_cb;
}

static inline void
fsapi_inode_complete(struct v_inode* inode, void* data)
{
    assert_fs(inode->ops);
    assert_fs(inode->default_fops);
    assert_fs(inode->default_fops);

    inode->data = data;
}

static inline void
fsapi_inode_settime(struct v_inode* inode, 
                    time_t ctime, time_t mtime, time_t atime)
{
    inode->ctime = ctime;
    inode->mtime = mtime;
    inode->atime = atime;
}

static inline bbuf_t
fsblock_take(struct v_superblock* vsb, unsigned int block_id)
{
    return blkbuf_take(vsb->blks, block_id);
}

static inline void
fsblock_put(struct v_superblock* vsb, bbuf_t blkbuf)
{
    return blkbuf_put(blkbuf);
}


#endif /* __LUNAIX_FSAPI_H */
