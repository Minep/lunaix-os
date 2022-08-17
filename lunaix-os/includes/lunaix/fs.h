#ifndef __LUNAIX_VFS_H
#define __LUNAIX_VFS_H

#include <lunaix/clock.h>
#include <lunaix/device.h>
#include <lunaix/ds/btrie.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/ds/hstr.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/lru.h>
#include <lunaix/ds/mutex.h>
#include <lunaix/status.h>
#include <stdatomic.h>

#define VFS_NAME_MAXLEN 128
#define VFS_MAX_FD 32

#define VFS_IFDIR 0x1
#define VFS_IFFILE 0x2
#define VFS_IFSEQDEV 0x4
#define VFS_IFVOLDEV 0x8
#define VFS_IFSYMLINK 0x16

#define VFS_WALK_MKPARENT 0x1
#define VFS_WALK_FSRELATIVE 0x2
#define VFS_WALK_PARENT 0x4
#define VFS_WALK_NOFOLLOW 0x4

#define VFS_HASHTABLE_BITS 10
#define VFS_HASHTABLE_SIZE (1 << VFS_HASHTABLE_BITS)
#define VFS_HASH_MASK (VFS_HASHTABLE_SIZE - 1)
#define VFS_HASHBITS (32 - VFS_HASHTABLE_BITS)

#define FSTYPE_ROFS 0x1

#define VFS_VALID_CHAR(chr)                                                    \
    (('A' <= (chr) && (chr) <= 'Z') || ('a' <= (chr) && (chr) <= 'z') ||       \
     ('0' <= (chr) && (chr) <= '9') || (chr) == '.' || (chr) == '_' ||         \
     (chr) == '-')

extern struct hstr vfs_ddot;
extern struct hstr vfs_dot;

typedef uint32_t inode_t;

struct v_dnode;
struct v_inode;
struct v_superblock;
struct v_file;
struct v_fd;
struct pcache;

struct filesystem
{
    struct hlist_node fs_list;
    struct hstr fs_name;
    uint32_t types;
    int fs_id; // can be used to detect fs type on partition
    int (*mount)(struct v_superblock* vsb, struct v_dnode* mount_point);
    int (*unmount)(struct v_superblock* vsb);
};

struct v_superblock
{
    struct llist_header sb_list;
    struct device* dev;
    struct v_dnode* root;
    struct filesystem* fs;
    uint32_t iobuf_size;
    struct hbucket* i_cache;
    struct
    {
        uint32_t (*read_capacity)(struct v_superblock* vsb);
        uint32_t (*read_usage)(struct v_superblock* vsb);
    } ops;
};

struct dir_context
{
    int index;
    void* cb_data;
    void (*read_complete_callback)(struct dir_context* dctx,
                                   const char* name,
                                   const int len,
                                   const int dtype);
};

struct v_file_ops
{
    int (*write)(struct v_inode* inode, void* buffer, size_t len, size_t fpos);
    int (*read)(struct v_inode* inode, void* buffer, size_t len, size_t fpos);
    int (*readdir)(struct v_inode* inode, struct dir_context* dctx);
    int (*seek)(struct v_inode* inode, size_t offset);
    int (*close)(struct v_file* file);
    int (*sync)(struct v_inode* inode);
};

struct v_file
{
    struct v_inode* inode;
    struct v_dnode* dnode;
    struct llist_header* f_list;
    uint32_t f_pos;
    atomic_ulong ref_count;
    struct v_file_ops ops;
};

struct v_fd
{
    struct v_file* file;
    int flags;
};

struct v_inode
{
    inode_t id;
    mutex_t lock;
    uint32_t itype;
    time_t ctime;
    time_t mtime;
    time_t atime;
    lba_t lb_addr;
    uint32_t open_count;
    uint32_t link_count;
    uint32_t lb_usage;
    uint32_t fsize;
    struct hlist_node hash_list;
    struct lru_node lru;
    struct pcache* pg_cache;
    void* data; // 允许底层FS绑定他的一些专有数据
    struct
    {
        int (*create)(struct v_inode* this, struct v_dnode* dnode);
        int (*open)(struct v_inode* this, struct v_file* file);
        int (*sync)(struct v_inode* this);
        int (*mkdir)(struct v_inode* this, struct v_dnode* dnode);
        int (*rmdir)(struct v_inode* this, struct v_dnode* dir);
        int (*unlink)(struct v_inode* this);
        int (*link)(struct v_inode* this, struct v_dnode* new_name);
        int (*read_symlink)(struct v_inode* this, const char** path_out);
        int (*set_symlink)(struct v_inode* this, const char* target);
        int (*dir_lookup)(struct v_inode* this, struct v_dnode* dnode);
        int (*rename)(struct v_inode* from_inode,
                      struct v_dnode* from_dnode,
                      struct v_dnode* to_dnode);
    } ops;
    struct v_file_ops default_fops;
};

struct v_mount
{
    mutex_t lock;
    struct llist_header list;
    struct llist_header submnts;
    struct llist_header sibmnts;
    struct v_mount* parent;
    struct v_dnode* mnt_point;
    struct v_superblock* super_block;
    uint32_t busy_counter;
};

struct v_dnode
{
    mutex_t lock; // sync the path walking
    struct lru_node lru;
    struct hstr name;
    struct v_inode* inode;
    struct v_dnode* parent;
    struct hlist_node hash_list;
    struct llist_header children;
    struct llist_header siblings;
    struct v_superblock* super_block;
    struct v_mount* mnt;
    atomic_ulong ref_count;

    void* data;
};

struct v_fdtable
{
    struct v_fd* fds[VFS_MAX_FD];
};

struct pcache
{
    struct v_inode* master;
    struct btrie tree;
    struct llist_header pages;
    struct llist_header dirty;
    uint32_t n_dirty;
    uint32_t n_pages;
};

struct pcache_pg
{
    struct llist_header pg_list;
    struct llist_header dirty_list;
    struct lru_node lru;
    struct pcache* holder;
    void* pg;
    uint32_t flags;
    uint32_t fpos;
};
/* --- file system manager --- */
void
fsm_init();

void
fsm_register(struct filesystem* fs);

struct filesystem*
fsm_get(const char* fs_name);

void
vfs_init();

struct v_dnode*
vfs_dcache_lookup(struct v_dnode* parent, struct hstr* str);

void
vfs_dcache_add(struct v_dnode* parent, struct v_dnode* dnode);

int
vfs_walk(struct v_dnode* start,
         const char* path,
         struct v_dnode** dentry,
         struct hstr* component,
         int walk_options);

int
vfs_mount(const char* target, const char* fs_name, struct device* device);

int
vfs_unmount(const char* target);

int
vfs_mount_at(const char* fs_name,
             struct device* device,
             struct v_dnode* mnt_point);

int
vfs_unmount_at(struct v_dnode* mnt_point);

int
vfs_mkdir(const char* path, struct v_dnode** dentry);

int
vfs_open(struct v_dnode* dnode, struct v_file** file);

int
vfs_close(struct v_file* file);

int
vfs_fsync(struct v_file* file);

void
vfs_assign_inode(struct v_dnode* assign_to, struct v_inode* inode);

struct v_superblock*
vfs_sb_alloc();

void
vfs_sb_free(struct v_superblock* sb);

struct v_dnode*
vfs_d_alloc();

void
vfs_d_free(struct v_dnode* dnode);

struct v_inode*
vfs_i_alloc(struct v_superblock* sb,
            uint32_t inode_id,
            void (*init)(struct v_inode* inode, void* data),
            void* data);

void
vfs_i_free(struct v_inode* inode);

int
vfs_dup_fd(struct v_fd* old, struct v_fd** new);

void
pcache_init(struct pcache* pcache);

void
pcache_release_page(struct pcache* pcache, struct pcache_pg* page);

struct pcache_pg*
pcache_new_page(struct pcache* pcache, uint32_t index);

void
pcache_set_dirty(struct pcache* pcache, struct pcache_pg* pg);

struct pcache_pg*
pcache_get_page(struct pcache* pcache,
                uint32_t index,
                uint32_t* offset,
                struct pcache_pg** page);

int
pcache_write(struct v_inode* inode, void* data, uint32_t len, uint32_t fpos);

int
pcache_read(struct v_inode* inode, void* data, uint32_t len, uint32_t fpos);

void
pcache_release(struct pcache* pcache);

int
pcache_commit(struct v_inode* inode, struct pcache_pg* page);

void
pcache_commit_all(struct v_inode* inode);

void
pcache_invalidate(struct pcache* pcache, struct pcache_pg* page);

/**
 * @brief 将挂载点标记为繁忙
 *
 * @param mnt
 */
void
mnt_mkbusy(struct v_mount* mnt);

/**
 * @brief 将挂载点标记为清闲
 *
 * @param mnt
 */
void
mnt_chillax(struct v_mount* mnt);

struct v_mount*
vfs_create_mount(struct v_mount* parent, struct v_dnode* mnt_point);
#endif /* __LUNAIX_VFS_H */
