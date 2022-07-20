#ifndef __LUNAIX_VFS_H
#define __LUNAIX_VFS_H

#include <hal/ahci/hba.h>
#include <lunaix/block.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/ds/hstr.h>
#include <lunaix/ds/llist.h>

#define VFS_NAME_MAXLEN 128

struct v_dnode;

struct filesystem
{
    struct hlist_node fs_list;
    struct hstr fs_name;
    int (*mount)(struct v_superblock* vsb, struct v_dnode* mount_point);
    int (*unmount)(struct v_superblock* vsb);
};

struct v_superblock
{
    struct llist_header sb_list;
    int fs_id;
    bdev_t dev;
    struct v_dnode* root;
    struct filesystem* fs;
    struct
    {
        uint32_t (*read_capacity)(struct v_superblock* vsb);
        uint32_t (*read_usage)(struct v_superblock* vsb);
    } ops;
};

struct v_file
{
    struct v_inode* inode;
    struct
    {
        void* data;
        uint32_t size;
        uint64_t lb_addr;
        uint32_t offset;
        int dirty;
    } buffer;
    struct
    {
        int (*write)(struct v_file* file, void* data_in, uint32_t size);
        int (*read)(struct v_file* file, void* data_out, uint32_t size);
        int (*readdir)(struct v_file* file, int dir_index);
        int (*seek)(struct v_file* file, size_t offset);
        int (*rename)(struct v_file* file, char* new_name);
        int (*close)(struct v_file* file);
        int (*sync)(struct v_file* file);
    } ops;
};

struct v_inode
{
    uint32_t itype;
    uint32_t ctime;
    uint32_t mtime;
    uint64_t lb_addr;
    uint32_t ref_count;
    uint32_t lb_usage;
    struct
    {
        int (*open)(struct v_inode* inode, struct v_file* file);
        int (*sync)(struct v_inode* inode);
        int (*mkdir)(struct v_inode* inode, struct v_dnode* dnode);
        int (*dir_lookup)(struct v_inode* inode, struct v_dnode* dnode);
    } ops;
};

struct v_dnode
{
    struct hstr name;
    struct v_inode* inode;
    struct v_dnode* parent;
    struct hlist_node hash_list;
    struct llist_header children;
    struct llist_header siblings;
    struct v_superblock* super_block;
};

/* --- file system manager --- */
void
fsm_init();

void
fsm_register(struct filesystem* fs);

struct filesystem*
fsm_get(const char* fs_name);

#endif /* __LUNAIX_VFS_H */
