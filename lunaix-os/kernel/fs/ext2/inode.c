#include <lunaix/fs/api.h>
#include <lunaix/mm/valloc.h>

#include "ext2.h"

#define IMODE_IFSOCK    0xc000
#define IMODE_IFLNK     0xA000
#define IMODE_IFREG     0x8000
#define IMODE_IFBLK     0x6000
#define IMODE_IFDIR     0x4000
#define IMODE_IFCHR     0x2000
#define IMODE_IFFIFO    0x1000

static int
ext2_inode_read(struct v_inode *inode, void *buffer, size_t len, size_t fpos)
{
    // TODO
    return 0;
}

static int
ext2_inode_read_page(struct v_inode *inode, void *buffer, size_t fpos)
{
    // TODO
    return 0;
}

static int
ext2_inode_write(struct v_inode *inode, void *buffer, size_t len, size_t fpos)
{
    // TODO
    return 0;
}

static int
ext2_inode_write_page(struct v_inode *inode, void *buffer, size_t fpos)
{
    // TODO
    return 0;
}

static struct v_inode_ops ext2_inode_ops = {
    .dir_lookup = ext2_dir_lookup,
    .open = ext2_open_inode,
};

static struct v_file_ops ext2_file_ops = {
    .close = ext2_close_inode,
    
    .read = ext2_inode_read,
    .read_page = ext2_inode_read_page,
    
    .write = ext2_inode_write,
    .write_page = ext2_inode_write_page,
    
    .readdir = ext2_dir_read,
    .seek = NULL,
    .sync = ext2_sync_inode
};

void
ext2_init_inode(struct v_superblock* vsb, struct v_inode* inode)
{
    // Perhaps we dont need this at all...
}

void
ext2_fill_inode(struct v_superblock* vsb, struct v_inode* inode, ino_t ino_id)
{
    struct ext2_sbinfo* sb;
    struct ext2_inode* ext2inode;
    struct ext2b_inode* b_ino;
    unsigned int type;

    sb = EXT2_SB(vsb);
    ext2inode = ext2_get_inode(vsb, ino_id);
    b_ino = &ext2inode->ino;
    
    fsapi_inode_setid(inode, ino_id, ino_id);
    fsapi_inode_setsize(inode, b_ino->i_size);
    
    fsapi_inode_settime(inode, b_ino->i_ctime, 
                               b_ino->i_mtime, 
                               b_ino->i_atime);
    
    fsapi_inode_setfops(ino_id, &ext2_file_ops);
    fsapi_inode_setops(ino_id, &ext2_inode_ops);

    if (b_ino->i_mode & IMODE_IFLNK) {
        type = VFS_IFSYMLINK;
    }
    else if (b_ino->i_mode & IMODE_IFDIR) {
        type = VFS_IFDIR;
    }
    else if (b_ino->i_mode & IMODE_IFCHR) {
        type = VFS_IFSEQDEV;
    }
    else if (b_ino->i_mode & IMODE_IFBLK) {
        type = VFS_IFVOLDEV;
    }

    if ((b_ino->i_mode & IMODE_IFREG)) {
        type |= VFS_IFFILE;
    }

    fsapi_inode_settype(inode, type);

    fsapi_inode_complete(inode, ext2inode);
}

struct ext2_inode*
ext2_get_inode(struct v_superblock* vsb, unsigned int ino_num)
{
    struct ext2_sbinfo* sb;
    struct ext2_inode* inode;
    struct ext2b_inode* b_inode;
    struct ext2b_gdesc* gd;
    unsigned int blkgrp_id, ino_rel_id;
    unsigned int ino_tab_sel, ino_tab_off, tab_partlen;
    bbuf_t ino_tab;

    sb = EXT2_SB(vsb);
    blkgrp_id   = ino_num / sb->raw.s_ino_per_grp;
    ino_rel_id  = ino_num % sb->raw.s_ino_per_grp;

    tab_partlen = sb->block_size / sb->raw.s_ino_size;
    ino_tab_sel = ino_rel_id / tab_partlen;
    ino_tab_off = ino_rel_id % tab_partlen;

    gd      = ext2gd_desc_at(vsb, blkgrp_id);
    ino_tab = fsapi_getblk_at(vsb, gd->bg_ino_tab + ino_tab_sel);
    b_inode = (struct ext2b_inode*)blkbuf_data(ino_tab);
    
    inode         = valloc(sizeof(*inode));
    inode->inotab = ino_tab;
    inode->ino    = &b_inode[ino_tab_off];
    
    return inode;
}