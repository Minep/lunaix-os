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
#include <lunaix/process.h>
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
#define VFS_WALK_NOFOLLOW 0x8

#define VFS_HASHTABLE_BITS 10
#define VFS_HASHTABLE_SIZE (1 << VFS_HASHTABLE_BITS)
#define VFS_HASH_MASK (VFS_HASHTABLE_SIZE - 1)
#define VFS_HASHBITS (32 - VFS_HASHTABLE_BITS)

#define VFS_PATH_DELIM '/'

#define FSTYPE_ROFS 0x1

#define DO_STATUS(errno) SYSCALL_ESTATUS(__current->k_status = errno)
#define DO_STATUS_OR_RETURN(errno) ({ errno < 0 ? DO_STATUS(errno) : errno; })

#define TEST_FD(fd) (fd >= 0 && fd < VFS_MAX_FD)

#define VFS_VALID_CHAR(chr)                                                    \
    (('A' <= (chr) && (chr) <= 'Z') || ('a' <= (chr) && (chr) <= 'z') ||       \
     ('0' <= (chr) && (chr) <= '9') || (chr) == '.' || (chr) == '_' ||         \
     (chr) == '-' || (chr) == ':')

#define unlock_inode(inode) mutex_unlock(&inode->lock)
#define lock_inode(inode)                                                      \
    ({                                                                         \
        mutex_lock(&inode->lock);                                              \
        lru_use_one(inode_lru, &inode->lru);                                   \
    })

#define unlock_dnode(dnode) mutex_unlock(&dnode->lock)
#define lock_dnode(dnode)                                                      \
    ({                                                                         \
        mutex_lock(&dnode->lock);                                              \
        lru_use_one(dnode_lru, &dnode->lru);                                   \
    })

typedef u32_t inode_t;

struct v_dnode;
struct v_inode;
struct v_superblock;
struct v_file;
struct v_file_ops;
struct v_inode_ops;
struct v_fd;
struct pcache;
struct v_xattr_entry;

extern struct v_file_ops default_file_ops;
extern struct v_inode_ops default_inode_ops;

extern struct hstr vfs_ddot;
extern struct hstr vfs_dot;
extern struct v_dnode* vfs_sysroot;

struct filesystem
{
    struct hlist_node fs_list;
    struct hstr fs_name;
    u32_t types;
    int fs_id;
    int (*mount)(struct v_superblock* vsb, struct v_dnode* mount_point);
    int (*unmount)(struct v_superblock* vsb);
};

struct v_superblock
{
    struct llist_header sb_list;
    struct device* dev;
    struct v_dnode* root;
    struct filesystem* fs;
    struct hbucket* i_cache;
    void* data;
    struct
    {
        u32_t (*read_capacity)(struct v_superblock* vsb);
        u32_t (*read_usage)(struct v_superblock* vsb);
        void (*init_inode)(struct v_superblock* vsb, struct v_inode* inode);
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

    // for operatiosn {write|read}_page, following are true:
    //  + `len` always equals to PG_SIZE
    //  + `fpos` always PG_SIZE aligned.
    // These additional operations allow underlying fs to use more specialized
    // and optimized code.

    int (*write_page)(struct v_inode* inode, void* pg, size_t len, size_t fpos);
    int (*read_page)(struct v_inode* inode, void* pg, size_t len, size_t fpos);

    int (*readdir)(struct v_file* file, struct dir_context* dctx);
    int (*seek)(struct v_inode* inode, size_t offset); // optional
    int (*close)(struct v_file* file);
    int (*sync)(struct v_file* file);
};

struct v_inode_ops
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
    int (*getxattr)(struct v_inode* this,
                    struct v_xattr_entry* entry); // optional
    int (*setxattr)(struct v_inode* this,
                    struct v_xattr_entry* entry); // optional
    int (*delxattr)(struct v_inode* this,
                    struct v_xattr_entry* entry); // optional
};

struct v_xattr_entry
{
    struct llist_header entries;
    struct hstr name;
    const void* value;
    size_t len;
};

struct v_file
{
    struct v_inode* inode;
    struct v_dnode* dnode;
    struct llist_header* f_list;
    u32_t f_pos;
    atomic_ulong ref_count;
    struct v_file_ops* ops; // for caching
};

struct v_fd
{
    struct v_file* file;
    int flags;
};

//  [v_inode::aka_nodes]
//  how do we invalidate corresponding v_dnodes given the v_inode?
/*
    Consider taskfs, which is Lunaix's speak of Linux's procfs, that allow
    info of every process being accessible via file system. Each process's
    creation will result a creation of a directory under the root of task fs
    with it's pid as name. But that dir must delete when process is killed, and
    such deletion does not mediated by vfs itself, so there is a need of cache
    syncing.
    And this is also the case of all ramfs where argumentation to file tree is
    performed by third party.
*/

struct v_inode
{
    inode_t id;
    mutex_t lock;
    u32_t itype;
    time_t ctime;
    time_t mtime;
    time_t atime;
    lba_t lb_addr;
    u32_t open_count;
    u32_t link_count;
    u32_t lb_usage;
    u32_t fsize;
    void* data; // 允许底层FS绑定他的一些专有数据
    struct llist_header aka_dnodes;
    struct llist_header xattrs;
    struct v_superblock* sb;
    struct hlist_node hash_list;
    struct lru_node lru;
    struct pcache* pg_cache;
    struct v_inode_ops* ops;
    struct v_file_ops* default_fops;

    void (*destruct)(struct v_inode* inode);
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
    u32_t busy_counter;
    int flags;
};

struct v_dnode
{
    mutex_t lock; // sync the path walking
    struct lru_node lru;
    struct hstr name;
    struct v_inode* inode;
    struct v_dnode* parent;
    struct hlist_node hash_list;
    struct llist_header aka_list;
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
    u32_t n_dirty;
    u32_t n_pages;
};

struct pcache_pg
{
    struct llist_header pg_list;
    struct llist_header dirty_list;
    struct lru_node lru;
    struct pcache* holder;
    void* pg;
    u32_t flags;
    u32_t fpos;
    u32_t len;
};

void
fsm_init();

void
fsm_register_all();

struct filesystem*
fsm_new_fs(char* name, size_t name_len);

void
fsm_register(struct filesystem* fs);

struct filesystem*
fsm_get(const char* fs_name);

void
vfs_init();

void
vfs_export_attributes();

struct v_dnode*
vfs_dcache_lookup(struct v_dnode* parent, struct hstr* str);

void
vfs_dcache_add(struct v_dnode* parent, struct v_dnode* dnode);

void
vfs_dcache_rehash(struct v_dnode* new_parent, struct v_dnode* dnode);

void
vfs_dcache_remove(struct v_dnode* dnode);

int
vfs_walk(struct v_dnode* start,
         const char* path,
         struct v_dnode** dentry,
         struct hstr* component,
         int walk_options);

int
vfs_walk_proc(const char* path,
              struct v_dnode** dentry,
              struct hstr* component,
              int options);

int
vfs_mount(const char* target,
          const char* fs_name,
          struct device* device,
          int options);

int
vfs_unmount(const char* target);

int
vfs_mount_at(const char* fs_name,
             struct device* device,
             struct v_dnode* mnt_point,
             int options);

int
vfs_unmount_at(struct v_dnode* mnt_point);

int
vfs_mkdir(const char* path, struct v_dnode** dentry);

int
vfs_open(struct v_dnode* dnode, struct v_file** file);

int
vfs_pclose(struct v_file* file, pid_t pid);

int
vfs_close(struct v_file* file);

void
vfs_free_fd(struct v_fd* fd);

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
vfs_i_find(struct v_superblock* sb, u32_t i_id);

void
vfs_i_addhash(struct v_inode* inode);

struct v_inode*
vfs_i_alloc(struct v_superblock* sb);

void
vfs_i_free(struct v_inode* inode);

int
vfs_dup_fd(struct v_fd* old, struct v_fd** new);

int
vfs_getfd(int fd, struct v_fd** fd_s);

int
vfs_get_dtype(int itype);

void
vfs_ref_dnode(struct v_dnode* dnode);

void
vfs_unref_dnode(struct v_dnode* dnode);

int
vfs_get_path(struct v_dnode* dnode, char* buf, size_t size, int depth);

void
pcache_init(struct pcache* pcache);

void
pcache_release_page(struct pcache* pcache, struct pcache_pg* page);

struct pcache_pg*
pcache_new_page(struct pcache* pcache, u32_t index);

void
pcache_set_dirty(struct pcache* pcache, struct pcache_pg* pg);

int
pcache_get_page(struct pcache* pcache,
                u32_t index,
                u32_t* offset,
                struct pcache_pg** page);

int
pcache_write(struct v_inode* inode, void* data, u32_t len, u32_t fpos);

int
pcache_read(struct v_inode* inode, void* data, u32_t len, u32_t fpos);

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

int
vfs_mount_root(const char* fs_name, struct device* device);

struct v_mount*
vfs_create_mount(struct v_mount* parent, struct v_dnode* mnt_point);

int
vfs_check_writable(struct v_dnode* dnode);

int
default_file_read(struct v_inode* inode, void* buffer, size_t len, size_t fpos);

int
default_file_write(struct v_inode* inode,
                   void* buffer,
                   size_t len,
                   size_t fpos);

int
default_file_readdir(struct v_file* file, struct dir_context* dctx);

int
default_inode_dirlookup(struct v_inode* this, struct v_dnode* dnode);

int
default_inode_rename(struct v_inode* from_inode,
                     struct v_dnode* from_dnode,
                     struct v_dnode* to_dnode);

int
default_file_close(struct v_file* file);

int
default_file_seek(struct v_inode* inode, size_t offset);

int
default_inode_open(struct v_inode* this, struct v_file* file);

int
default_inode_rmdir(struct v_inode* this, struct v_dnode* dir);

int
default_inode_mkdir(struct v_inode* this, struct v_dnode* dir);

struct v_xattr_entry*
xattr_new(struct hstr* name);

struct v_xattr_entry*
xattr_getcache(struct v_inode* inode, struct hstr* name);

void
xattr_addcache(struct v_inode* inode, struct v_xattr_entry* xattr);

#endif /* __LUNAIX_VFS_H */
