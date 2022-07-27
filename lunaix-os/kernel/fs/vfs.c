/**
 * @file vfs.c
 * @author Lunaixsky (zelong56@gmail.com)
 * @brief Lunaix virtual file system - an abstraction layer for all file system.
 * @version 0.1
 * @date 2022-07-24
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <klibc/string.h>
#include <lunaix/dirent.h>
#include <lunaix/foptions.h>
#include <lunaix/fs.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>

#define PATH_DELIM '/'
#define DNODE_HASHTABLE_BITS 10
#define DNODE_HASHTABLE_SIZE (1 << DNODE_HASHTABLE_BITS)
#define DNODE_HASH_MASK (DNODE_HASHTABLE_SIZE - 1)
#define DNODE_HASHBITS (32 - DNODE_HASHTABLE_BITS)

static struct cake_pile* dnode_pile;
static struct cake_pile* inode_pile;
static struct cake_pile* file_pile;
static struct cake_pile* superblock_pile;
static struct cake_pile* fd_pile;

static struct v_superblock* root_sb;
static struct hbucket* dnode_cache;

static int fs_id = 0;

struct v_dnode*
vfs_d_alloc();

void
vfs_d_free(struct v_dnode* dnode);

struct v_superblock*
vfs_sb_alloc();

void
vfs_sb_free(struct v_superblock* sb);

void
vfs_init()
{
    // 为他们专门创建一个蛋糕堆，而不使用valloc，这样我们可以最小化内碎片的产生
    dnode_pile = cake_new_pile("dnode_cache", sizeof(struct v_dnode), 1, 0);
    inode_pile = cake_new_pile("inode_cache", sizeof(struct v_inode), 1, 0);
    file_pile = cake_new_pile("file_cache", sizeof(struct v_file), 1, 0);
    fd_pile = cake_new_pile("fd_cache", sizeof(struct v_fd), 1, 0);
    superblock_pile =
      cake_new_pile("sb_cache", sizeof(struct v_superblock), 1, 0);

    dnode_cache = vzalloc(DNODE_HASHTABLE_SIZE * sizeof(struct hbucket));

    // 创建一个根superblock，用来蕴含我们的根目录。
    root_sb = vfs_sb_alloc();
    root_sb->root = vfs_d_alloc();
}

inline struct hbucket*
__dcache_get_bucket(struct v_dnode* parent, unsigned int hash)
{
    // 与parent的指针值做加法，来减小碰撞的可能性。
    hash += (uint32_t)parent;
    // 确保低位更加随机
    hash = hash ^ (hash >> DNODE_HASHBITS);
    return &dnode_cache[hash & DNODE_HASH_MASK];
}

struct v_dnode*
vfs_dcache_lookup(struct v_dnode* parent, struct hstr* str)
{
    if (!str->len)
        return parent;

    struct hbucket* slot = __dcache_get_bucket(parent, str->hash);

    struct v_dnode *pos, *n;
    hashtable_bucket_foreach(slot, pos, n, hash_list)
    {
        if (pos->name.hash == str->hash) {
            return pos;
        }
    }
    return NULL;
}

void
vfs_dcache_add(struct v_dnode* parent, struct v_dnode* dnode)
{
    struct hbucket* bucket = __dcache_get_bucket(parent, dnode->name.hash);
    hlist_add(&bucket->head, &dnode->hash_list);
}

int
vfs_walk(struct v_dnode* start,
         const char* path,
         struct v_dnode** dentry,
         struct hstr* component,
         int walk_options)
{
    int errno = 0;
    int i = 0, j = 0;

    if (path[0] == PATH_DELIM || !start) {
        if ((walk_options & VFS_WALK_FSRELATIVE) && start) {
            start = start->super_block->root;
        } else {
            start = root_sb->root;
        }
        i++;
    }

    struct v_dnode* dnode;
    struct v_dnode* current_level = start;

    char name_content[VFS_NAME_MAXLEN];
    struct hstr name = HSTR(name_content, 0);

    char current = path[i++], lookahead;
    while (current) {
        lookahead = path[i++];
        if (current != PATH_DELIM) {
            if (j >= VFS_NAME_MAXLEN - 1) {
                return ENAMETOOLONG;
            }
            if (!VFS_VALID_CHAR(current)) {
                return VFS_EINVLD;
            }
            name_content[j++] = current;
            if (lookahead) {
                goto cont;
            }
        }

        // handling cases like /^.*(\/+).*$/
        if (lookahead == PATH_DELIM) {
            goto cont;
        }

        name_content[j] = 0;
        name.len = j;
        hstr_rehash(&name, HSTR_FULL_HASH);

        if (!lookahead && (walk_options & VFS_WALK_PARENT)) {
            if (component) {
                component->hash = name.hash;
                component->len = j;
                strcpy(component->value, name_content);
            }
            break;
        }

        dnode = vfs_dcache_lookup(current_level, &name);

        if (!dnode) {
            dnode = vfs_d_alloc();
            dnode->name = HSTR(valloc(VFS_NAME_MAXLEN), j);
            dnode->name.hash = name.hash;

            strcpy(dnode->name.value, name_content);

            errno =
              current_level->inode->ops.dir_lookup(current_level->inode, dnode);

            if (errno == ENOENT && (walk_options & VFS_WALK_MKPARENT)) {
                if (!current_level->inode->ops.mkdir) {
                    errno = ENOTSUP;
                } else {
                    errno = current_level->inode->ops.mkdir(
                      current_level->inode, dnode);
                }
            }

            if (errno) {
                goto error;
            }

            vfs_dcache_add(current_level, dnode);

            dnode->parent = current_level;
            llist_append(&current_level->children, &dnode->siblings);
        }

        j = 0;
        current_level = dnode;
    cont:
        current = lookahead;
    };

    *dentry = current_level;
    return 0;

error:
    vfree(dnode->name.value);
    vfs_d_free(dnode);
    return errno;
}

int
vfs_mount(const char* target, const char* fs_name, bdev_t device)
{
    int errno;
    struct v_dnode* mnt;

    if (!(errno = vfs_walk(NULL, target, &mnt, NULL, 0))) {
        errno = vfs_mount_at(fs_name, device, mnt);
    }

    return errno;
}

int
vfs_unmount(const char* target)
{
    int errno;
    struct v_dnode* mnt;

    if (!(errno = vfs_walk(NULL, target, &mnt, NULL, 0))) {
        errno = vfs_unmount_at(mnt);
    }

    return errno;
}

int
vfs_mount_at(const char* fs_name, bdev_t device, struct v_dnode* mnt_point)
{
    struct filesystem* fs = fsm_get(fs_name);
    if (!fs)
        return VFS_ENOFS;
    struct v_superblock* sb = vfs_sb_alloc();
    sb->dev = device;
    sb->fs_id = fs_id++;

    int errno = 0;
    if (!(errno = fs->mount(sb, mnt_point))) {
        sb->fs = fs;
        sb->root = mnt_point;
        mnt_point->super_block = sb;
        llist_append(&root_sb->sb_list, &sb->sb_list);
    }

    return errno;
}

int
vfs_unmount_at(struct v_dnode* mnt_point)
{
    int errno = 0;
    struct v_superblock* sb = mnt_point->super_block;
    if (!sb) {
        return VFS_EBADMNT;
    }
    if (!(errno = sb->fs->unmount(sb))) {
        struct v_dnode* fs_root = sb->root;
        llist_delete(&fs_root->siblings);
        llist_delete(&sb->sb_list);
        vfs_sb_free(sb);
    }
    return errno;
}

int
vfs_open(struct v_dnode* dnode, struct v_file** file)
{
    if (!dnode->inode || !dnode->inode->ops.open) {
        return ENOTSUP;
    }

    struct v_file* vfile = cake_grab(file_pile);
    memset(vfile, 0, sizeof(*vfile));

    int errno = dnode->inode->ops.open(dnode->inode, vfile);
    if (errno) {
        cake_release(file_pile, vfile);
    } else {
        *file = vfile;
    }
    return errno;
}

int
vfs_close(struct v_file* file)
{
    if (!file->ops.close) {
        return ENOTSUP;
    }

    int errno = file->ops.close(file);
    if (!errno) {
        cake_release(file_pile, file);
    }
    return errno;
}

int
vfs_fsync(struct v_file* file)
{
    int errno = ENOTSUP;
    if (file->ops.sync) {
        errno = file->ops.sync(file);
    }
    if (!errno && file->inode->ops.sync) {
        return file->inode->ops.sync(file->inode);
    }
    return errno;
}

int
vfs_alloc_fdslot(int* fd)
{
    for (size_t i = 0; i < VFS_MAX_FD; i++) {
        if (!__current->fdtable->fds[i]) {
            *fd = i;
            return 0;
        }
    }
    return EMFILE;
}

struct v_superblock*
vfs_sb_alloc()
{
    struct v_superblock* sb = cake_grab(superblock_pile);
    memset(sb, 0, sizeof(*sb));
    llist_init_head(&sb->sb_list);
    return sb;
}

void
vfs_sb_free(struct v_superblock* sb)
{
    cake_release(superblock_pile, sb);
}

struct v_dnode*
vfs_d_alloc()
{
    struct v_dnode* dnode = cake_grab(dnode_pile);
    llist_init_head(&dnode->children);
}

void
vfs_d_free(struct v_dnode* dnode)
{
    if (dnode->ops.destruct) {
        dnode->ops.destruct(dnode);
    }
    cake_release(dnode_pile, dnode);
}

struct v_inode*
vfs_i_alloc()
{
    struct v_inode* inode = cake_grab(inode_pile);
    memset(inode, 0, sizeof(*inode));

    return inode;
}

void
vfs_i_free(struct v_inode* inode)
{
    cake_release(inode_pile, inode);
}

__DEFINE_LXSYSCALL2(int, open, const char*, path, int, options)
{
    char name_str[VFS_NAME_MAXLEN];
    struct hstr name = HSTR(name_str, 0);
    struct v_dnode *dentry, *file;
    int errno, fd;
    if ((errno = vfs_walk(NULL, path, &dentry, &name, VFS_WALK_PARENT))) {
        return -1;
    }

    vfs_walk(dentry, name.value, &file, NULL, 0);

    struct v_file* opened_file = 0;
    if (!file) {
        if ((options & FO_CREATE)) {
            errno = dentry->inode->ops.create(dentry->inode, opened_file);
        } else {
            errno = ENOENT;
        }
    } else {
        errno = vfs_open(file, &opened_file);
    }

    __current->k_status = errno;

    if (!errno && !(errno = vfs_alloc_fdslot(&fd))) {
        struct v_fd* fd_s = vzalloc(sizeof(*fd_s));
        fd_s->file = opened_file;
        fd_s->pos = file->inode->fsize & -((options & FO_APPEND) == 0);
        __current->fdtable->fds[fd] = fd_s;
    }

    return SYSCALL_ESTATUS(errno);
}

__DEFINE_LXSYSCALL1(int, close, int, fd)
{
    struct v_fd* fd_s;
    int errno;
    if (fd < 0 || fd >= VFS_MAX_FD || !(fd_s = __current->fdtable->fds[fd])) {
        errno = EBADF;
    } else if (!(errno = vfs_close(fd_s->file))) {
        vfree(fd_s);
        __current->fdtable->fds[fd] = 0;
    }

    __current->k_status = errno;

    return SYSCALL_ESTATUS(errno);
}

void
__vfs_readdir_callback(struct dir_context* dctx,
                       const char* name,
                       const int len,
                       const int dtype)
{
    struct dirent* dent = (struct dirent*)dctx->cb_data;
    strncpy(dent->d_name, name, DIRENT_NAME_MAX_LEN);
    dent->d_nlen = len;
    dent->d_type = dtype;
}

__DEFINE_LXSYSCALL2(int, readdir, int, fd, struct dirent*, dent)
{
    struct v_fd* fd_s;
    int errno;
    if (fd < 0 || fd >= VFS_MAX_FD || !(fd_s = __current->fdtable->fds[fd])) {
        errno = EBADF;
    } else if (!(fd_s->file->inode->itype & VFS_INODE_TYPE_DIR)) {
        errno = ENOTDIR;
    } else {
        struct dir_context dctx =
          (struct dir_context){ .cb_data = dent,
                                .index = dent->d_offset,
                                .read_complete_callback =
                                  __vfs_readdir_callback };
        if (!(errno = fd_s->file->ops.readdir(fd_s->file, &dctx))) {
            dent->d_offset++;
        }
    }

    __current->k_status = errno;
    return SYSCALL_ESTATUS(errno);
}

__DEFINE_LXSYSCALL1(int, mkdir, const char*, path)
{
    struct v_dnode *parent, *dir;
    struct hstr component = HSTR(valloc(VFS_NAME_MAXLEN), 0);
    int errno = vfs_walk(NULL, path, &parent, &component, VFS_WALK_PARENT);
    if (errno) {
        goto done;
    }

    if (!parent->inode->ops.mkdir) {
        errno = ENOTSUP;
    } else if (!(parent->inode->itype & VFS_INODE_TYPE_DIR)) {
        errno = ENOTDIR;
    } else {
        dir = vfs_d_alloc();
        dir->name = component;
        if (!(errno = parent->inode->ops.mkdir(parent->inode, dir))) {
            llist_append(&parent->children, &dir->siblings);
        } else {
            vfs_d_free(dir);
            vfree(component.value);
        }
    }

done:
    __current->k_status = errno;
    return SYSCALL_ESTATUS(errno);
}

__DEFINE_LXSYSCALL3(size_t, read, int, fd, void*, buf, size_t, count)
{
    // TODO
}

__DEFINE_LXSYSCALL3(size_t, write, int, fd, void*, buf, size_t, count)
{
    // TODO
}