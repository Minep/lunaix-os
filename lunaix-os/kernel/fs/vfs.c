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

// Welcome to The Mountain O'Shit! :)

/*
 TODO vfs & device todos checklist

    It is overseen by Twilight Sparkle ;)

 1. Get inodes hooked into lru
 2. Get dnodes hooked into lru
 3. Get inodes properly hashed so they can be reused by underling fs (CHECKED)
 4. (lru) Add a callback function (or destructor) for eviction.
        [good idea] or a constructor/destructor pattern in cake allocator ?
 5. (mount) Figure out a way to identify a busy mount point before unmount
            maybe a unified mount_point structure that maintain a referencing
            counter on any dnodes within the subtree? Such a counter will only
            increament if a file is opened or a dnode is being used as working
            directory and decreamenting conversely.
 6. (mount) Ability to track all mount points (including sub-mounts)
            so we can be confident to clean up everything when we unmount.
 7. (mount) Figure out a way to acquire the device represented by a dnode.
            so it can be used to mount. (e.g. we wish to get `struct device*`
            out of the dnode at /dev/sda)
            [tip] we should pay attention at twifs and add a private_data field
            under struct v_dnode?
 8. (mount) Then, we should refactor on mount/unmount mechanism.
 9. (mount) (future) Ability to mount any thing? e.g. Linux can mount a disk
                    image file using a so called "loopback" pseudo device. Maybe
                    we can do similar thing in Lunaix? A block device emulation
                    above the regular file when we mount it on.
 10. (device) device number (dev_t) allocation
            [good idea] <class>:<subclass>:<uniq_id> composition
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

#include <lunaix/fs/twifs.h>

#define PATH_DELIM '/'
#define HASHTABLE_BITS 10
#define HASHTABLE_SIZE (1 << HASHTABLE_BITS)
#define HASH_MASK (HASHTABLE_SIZE - 1)
#define HASHBITS (32 - HASHTABLE_BITS)

#define lock_inode(inode) mutex_lock(&inode->lock)
#define unlock_inode(inode) mutex_unlock(&inode->lock)

#define lock_dnode(dnode) mutex_lock(&dnode->lock)
#define unlock_dnode(dnode) mutex_unlock(&dnode->lock)

static struct cake_pile* dnode_pile;
static struct cake_pile* inode_pile;
static struct cake_pile* file_pile;
static struct cake_pile* superblock_pile;
static struct cake_pile* fd_pile;

static struct v_superblock* root_sb;
static struct hbucket *dnode_cache, *inode_cache;

struct hstr vfs_ddot = HSTR("..", 2);
struct hstr vfs_dot = HSTR(".", 1);
struct hstr vfs_empty = HSTR("", 0);

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

    dnode_cache = vzalloc(HASHTABLE_SIZE * sizeof(struct hbucket));
    inode_cache = vzalloc(HASHTABLE_SIZE * sizeof(struct hbucket));

    hstr_rehash(&vfs_ddot, HSTR_FULL_HASH);
    hstr_rehash(&vfs_dot, HSTR_FULL_HASH);

    // 创建一个根superblock，用来蕴含我们的根目录。
    root_sb = vfs_sb_alloc();
    root_sb->root = vfs_d_alloc();
    root_sb->root->inode = vfs_i_alloc(root_sb, 0);
}

inline struct hbucket*
__dcache_hash(struct v_dnode* parent, uint32_t* hash)
{
    uint32_t _hash = *hash;
    // 与parent的指针值做加法，来减小碰撞的可能性。
    _hash += (uint32_t)parent;
    // 确保低位更加随机
    _hash = _hash ^ (_hash >> HASHBITS);
    *hash = _hash;
    return &dnode_cache[_hash & HASH_MASK];
}

struct v_dnode*
vfs_dcache_lookup(struct v_dnode* parent, struct hstr* str)
{
    if (!str->len || HSTR_EQ(str, &vfs_dot))
        return parent;

    if (HSTR_EQ(str, &vfs_ddot)) {
        return parent->parent ? parent->parent : parent;
    }

    uint32_t hash = str->hash;
    struct hbucket* slot = __dcache_hash(parent, &hash);

    struct v_dnode *pos, *n;
    hashtable_bucket_foreach(slot, pos, n, hash_list)
    {
        if (pos->name.hash == hash) {
            return pos;
        }
    }
    return NULL;
}

void
vfs_dcache_add(struct v_dnode* parent, struct v_dnode* dnode)
{
    atomic_fetch_add(&dnode->ref_count, 1);
    dnode->parent = parent;
    llist_append(&parent->children, &dnode->siblings);
    struct hbucket* bucket = __dcache_hash(parent, &dnode->name.hash);
    hlist_add(&bucket->head, &dnode->hash_list);
}

void
vfs_dcache_remove(struct v_dnode* dnode)
{
    assert(dnode->ref_count == 1);

    llist_delete(&dnode->siblings);
    hlist_delete(&dnode->hash_list);

    dnode->parent = NULL;
    atomic_fetch_sub(&dnode->ref_count, 1);
}

void
vfs_dcache_rehash(struct v_dnode* new_parent, struct v_dnode* dnode)
{
    hstr_rehash(&dnode->name, HSTR_FULL_HASH);
    vfs_dcache_remove(dnode);
    vfs_dcache_add(new_parent, dnode);
}

int
__vfs_walk(struct v_dnode* start,
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
                return EINVAL;
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

        lock_dnode(current_level);

        name_content[j] = 0;
        name.len = j;
        hstr_rehash(&name, HSTR_FULL_HASH);

        if (!lookahead && (walk_options & VFS_WALK_PARENT)) {
            if (component) {
                component->hash = name.hash;
                component->len = j;
                strcpy(component->value, name_content);
            }
            unlock_dnode(current_level);
            break;
        }

        dnode = vfs_dcache_lookup(current_level, &name);

        if (!dnode) {
            dnode = vfs_d_alloc();

            hstrcpy(&dnode->name, &name);

            lock_inode(current_level->inode);

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

            unlock_inode(current_level->inode);

            if (errno) {
                unlock_dnode(current_level);
                vfree(dnode->name.value);
                goto error;
            }

            vfs_dcache_add(current_level, dnode);
        }

        unlock_dnode(current_level);

        j = 0;
        current_level = dnode;
    cont:
        current = lookahead;
    };

    *dentry = current_level;
    return 0;

error:
    vfs_d_free(dnode);
    *dentry = NULL;
    return errno;
}

#define VFS_MAX_SYMLINK 16

int
vfs_walk(struct v_dnode* start,
         const char* path,
         struct v_dnode** dentry,
         struct hstr* component,
         int options)
{
    struct v_dnode* interim;
    const char* pathname = path;
    int errno = __vfs_walk(start, path, &interim, component, options);
    int counter = 0;

    while (!errno) {
        if (counter >= VFS_MAX_SYMLINK) {
            errno = ELOOP;
            continue;
        }
        if ((interim->inode->itype & VFS_IFSYMLINK) &&
            !(options & VFS_WALK_NOFOLLOW) &&
            interim->inode->ops.read_symlink) {
            errno = interim->inode->ops.read_symlink(interim->inode, &pathname);
            if (errno) {
                break;
            }
        } else {
            break;
        }
        errno = __vfs_walk(start, pathname, &interim, component, options);
        counter++;
    }

    *dentry = errno ? 0 : interim;

    return errno;
}

int
vfs_mount(const char* target, const char* fs_name, struct device* device)
{
    int errno;
    struct v_dnode* mnt;

    if (!(errno = vfs_walk(__current->cwd, target, &mnt, NULL, 0))) {
        errno = vfs_mount_at(fs_name, device, mnt);
    }

    return errno;
}

int
vfs_unmount(const char* target)
{
    int errno;
    struct v_dnode* mnt;

    if (!(errno = vfs_walk(__current->cwd, target, &mnt, NULL, 0))) {
        errno = vfs_unmount_at(mnt);
    }

    return errno;
}

int
vfs_mount_at(const char* fs_name,
             struct device* device,
             struct v_dnode* mnt_point)
{
    if (!(mnt_point->inode->itype & VFS_IFDIR)) {
        return ENOTDIR;
    }

    struct filesystem* fs = fsm_get(fs_name);
    if (!fs) {
        return ENODEV;
    }

    struct v_superblock* sb = vfs_sb_alloc();
    sb->dev = device;
    sb->fs_id = fs->fs_id;

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
    // FIXME deal with the detached dcache subtree
    int errno = 0;
    struct v_superblock* sb = mnt_point->super_block;
    if (!sb) {
        return EINVAL;
    }

    if (sb->root != mnt_point) {
        return EINVAL;
    }

    if (!(errno = sb->fs->unmount(sb))) {
        struct v_dnode* fs_root = sb->root;
        vfs_dcache_remove(fs_root);

        llist_delete(&sb->sb_list);
        vfs_sb_free(sb);
        vfs_d_free(fs_root);
    }
    return errno;
}

int
vfs_open(struct v_dnode* dnode, struct v_file** file)
{
    if (!dnode->inode || !dnode->inode->ops.open) {
        return ENOTSUP;
    }

    struct v_inode* inode = dnode->inode;
    struct v_file* vfile = cake_grab(file_pile);
    memset(vfile, 0, sizeof(*vfile));

    vfile->dnode = dnode;
    vfile->inode = inode;
    vfile->ref_count = ATOMIC_VAR_INIT(1);
    vfile->ops = inode->default_fops;

    if ((inode->itype & VFS_IFFILE) && !inode->pg_cache) {
        struct pcache* pcache = vzalloc(sizeof(struct pcache));
        pcache_init(pcache);
        pcache->master = inode;
        inode->pg_cache = pcache;
    }

    int errno = inode->ops.open(inode, vfile);
    if (errno) {
        cake_release(file_pile, vfile);
    } else {
        atomic_fetch_add(&dnode->ref_count, 1);
        inode->open_count++;

        *file = vfile;
    }

    return errno;
}

int
vfs_link(struct v_dnode* to_link, struct v_dnode* name)
{
    int errno;

    lock_inode(to_link->inode);
    if (to_link->super_block->root != name->super_block->root) {
        errno = EXDEV;
    } else if (!to_link->inode->ops.link) {
        errno = ENOTSUP;
    } else if (!(errno = to_link->inode->ops.link(to_link->inode, name))) {
        name->inode = to_link->inode;
        to_link->inode->link_count++;
    }
    unlock_inode(to_link->inode);

    return errno;
}

int
vfs_close(struct v_file* file)
{
    int errno = 0;
    if (!file->ops.close || !(errno = file->ops.close(file))) {
        atomic_fetch_sub(&file->dnode->ref_count, 1);
        file->inode->open_count--;

        pcache_commit_all(file->inode);
        cake_release(file_pile, file);
    }
    return errno;
}

int
vfs_fsync(struct v_file* file)
{
    lock_inode(file->inode);

    int errno = ENOTSUP;
    pcache_commit_all(file->inode);
    if (file->ops.sync) {
        errno = file->ops.sync(file->inode);
    }

    unlock_inode(file->inode);

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
    memset(dnode, 0, sizeof(*dnode));
    llist_init_head(&dnode->children);
    llist_init_head(&dnode->siblings);
    mutex_init(&dnode->lock);

    dnode->ref_count = ATOMIC_VAR_INIT(0);
    dnode->name = HHSTR(vzalloc(VFS_NAME_MAXLEN), 0, 0);

    return dnode;
}

void
vfs_d_free(struct v_dnode* dnode)
{
    if (dnode->ref_count) {
        // it can be only freed if no one is refering
        return;
    }
    if (dnode->inode && dnode->inode->link_count) {
        dnode->inode->link_count--;
    }

    // Make sure the children de-referencing their parent.
    // With lru presented, the eviction will be propagated over the entire
    // detached subtree eventually
    struct v_dnode *pos, *n;
    llist_for_each(pos, n, &dnode->children, siblings)
    {
        vfs_dcache_remove(pos);
    }

    vfree(dnode->name.value);
    cake_release(dnode_pile, dnode);
}

struct v_inode*
vfs_i_alloc(dev_t device_id, uint32_t inode_id)
{
    // 我们这里假设每个文件系统与设备是一一对应（毕竟一个分区不可能有两个不同的文件系统）
    // 而每个文件系统所产生的 v_inode 缓存必须要和其他文件系统产生的区分开来。
    // 这也就是说，每个 v_inode 的 id
    // 必须要由设备ID，和该虚拟inode缓存所对应的物理inode
    // 相对于其所在的文件系统的id，进行组成！
    inode_id = hash_32(inode_id ^ device_id, HASH_SIZE_BITS);
    inode_id = (inode_id >> HASHBITS) ^ inode_id;

    struct hbucket* slot = &inode_cache[inode_id & HASH_MASK];
    struct v_inode *pos, *n;
    hashtable_bucket_foreach(slot, pos, n, hash_list)
    {
        if (pos->id == inode_id) {
            return pos;
        }
    }

    pos = cake_grab(inode_pile);
    memset(pos, 0, sizeof(*pos));

    pos->id = inode_id;
    pos->link_count = 1;

    mutex_init(&pos->lock);

    hlist_add(&slot->head, &pos->hash_list);

    return pos;
}

void
vfs_i_free(struct v_inode* inode)
{
    hlist_delete(&inode->hash_list);
    cake_release(inode_pile, inode);
}

/* ---- System call definition and support ---- */

#define FLOCATE_CREATE_EMPTY 1

#define DO_STATUS(errno) SYSCALL_ESTATUS(__current->k_status = errno)
#define DO_STATUS_OR_RETURN(errno) ({ errno < 0 ? DO_STATUS(errno) : errno; })

#define TEST_FD(fd) (fd >= 0 && fd < VFS_MAX_FD)

int
__vfs_getfd(int fd, struct v_fd** fd_s)
{
    if (TEST_FD(fd) && (*fd_s = __current->fdtable->fds[fd])) {
        return 0;
    }
    return EBADF;
}

int
__vfs_try_locate_file(const char* path,
                      struct v_dnode** fdir,
                      struct v_dnode** file,
                      int options)
{
    char name_str[VFS_NAME_MAXLEN];
    struct hstr name = HSTR(name_str, 0);
    int errno;
    if ((errno =
           vfs_walk(__current->cwd, path, fdir, &name, VFS_WALK_PARENT))) {
        return errno;
    }

    errno = vfs_walk(*fdir, name.value, file, NULL, 0);
    if (errno != ENOENT || !(options & FLOCATE_CREATE_EMPTY)) {
        return errno;
    }

    struct v_dnode* parent = *fdir;
    struct v_dnode* file_new = vfs_d_alloc();
    hstrcpy(&file_new->name, &name);

    if (!(errno = parent->inode->ops.create(parent->inode, file_new))) {
        *file = file_new;

        vfs_dcache_add(parent, file_new);
        llist_append(&parent->children, &file_new->siblings);
    } else {
        vfs_d_free(file_new);
    }

    return errno;
}

int
vfs_do_open(const char* path, int options)
{
    int errno, fd;
    struct v_dnode *dentry, *file;
    struct v_file* ofile = 0;

    errno = __vfs_try_locate_file(
      path, &dentry, &file, (options & FO_CREATE) ? FLOCATE_CREATE_EMPTY : 0);

    if (errno || (errno = vfs_open(file, &ofile))) {
        return errno;
    }

    struct v_inode* o_inode = ofile->inode;
    if (!(o_inode->itype & VFS_IFSEQDEV) && !(options & FO_DIRECT)) {
        // XXX Change here accordingly when signature of pcache_r/w changed.
        ofile->ops.read = pcache_read;
        ofile->ops.write = pcache_write;
    }

    if (!errno && !(errno = vfs_alloc_fdslot(&fd))) {
        struct v_fd* fd_s = vzalloc(sizeof(*fd_s));
        ofile->f_pos = ofile->inode->fsize & -((options & FO_APPEND) != 0);
        fd_s->file = ofile;
        fd_s->flags = options;
        __current->fdtable->fds[fd] = fd_s;
        return fd;
    }

    return errno;
}

__DEFINE_LXSYSCALL2(int, open, const char*, path, int, options)
{
    int errno = vfs_do_open(path, options);
    return DO_STATUS_OR_RETURN(errno);
}

__DEFINE_LXSYSCALL1(int, close, int, fd)
{
    struct v_fd* fd_s;
    int errno = 0;
    if ((errno = __vfs_getfd(fd, &fd_s))) {
        goto done_err;
    }

    if (fd_s->file->ref_count > 1) {
        fd_s->file->ref_count--;
    } else if ((errno = vfs_close(fd_s->file))) {
        goto done_err;
    }

    vfree(fd_s);
    __current->fdtable->fds[fd] = 0;

done_err:
    return DO_STATUS(errno);
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

    if ((errno = __vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct v_inode* inode = fd_s->file->inode;

    lock_inode(inode);

    if (!(fd_s->file->inode->itype & VFS_IFDIR)) {
        errno = ENOTDIR;
    } else {
        struct dir_context dctx =
          (struct dir_context){ .cb_data = dent,
                                .index = dent->d_offset,
                                .read_complete_callback =
                                  __vfs_readdir_callback };
        if (dent->d_offset == 0) {
            __vfs_readdir_callback(&dctx, vfs_dot.value, vfs_dot.len, 0);
        } else if (dent->d_offset == 1) {
            __vfs_readdir_callback(&dctx, vfs_ddot.value, vfs_ddot.len, 0);
        } else {
            dctx.index -= 2;
            if ((errno = fd_s->file->ops.readdir(inode, &dctx))) {
                unlock_inode(inode);
                goto done;
            }
        }
        errno = 0;
        dent->d_offset++;
    }

    unlock_inode(inode);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL3(int, read, int, fd, void*, buf, size_t, count)
{
    int errno = 0;
    struct v_fd* fd_s;
    if ((errno = __vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct v_file* file = fd_s->file;
    if ((file->inode->itype & VFS_IFDIR)) {
        errno = EISDIR;
        goto done;
    }

    lock_inode(file->inode);

    file->inode->atime = clock_unixtime();

    __SYSCALL_INTERRUPTIBLE(
      { errno = file->ops.read(file->inode, buf, count, file->f_pos); })

    unlock_inode(file->inode);

    if (errno > 0) {
        file->f_pos += errno;
        return errno;
    }

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL3(int, write, int, fd, void*, buf, size_t, count)
{
    int errno = 0;
    struct v_fd* fd_s;
    if ((errno = __vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct v_file* file = fd_s->file;
    if ((file->inode->itype & VFS_IFDIR)) {
        errno = EISDIR;
        goto done;
    }

    lock_inode(file->inode);

    file->inode->mtime = clock_unixtime();

    __SYSCALL_INTERRUPTIBLE(
      { errno = file->ops.write(file->inode, buf, count, file->f_pos); })

    unlock_inode(file->inode);

    if (errno > 0) {
        file->f_pos += errno;
        return errno;
    }

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL3(int, lseek, int, fd, int, offset, int, options)
{
    int errno = 0;
    struct v_fd* fd_s;
    if ((errno = __vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct v_file* file = fd_s->file;

    lock_inode(file->inode);

    size_t fpos = file->f_pos;
    switch (options) {
        case FSEEK_CUR:
            fpos = (size_t)((int)file->f_pos + offset);
            break;
        case FSEEK_END:
            fpos = (size_t)((int)file->inode->fsize + offset);
            break;
        case FSEEK_SET:
            fpos = offset;
            break;
    }
    if (!file->ops.seek || !(errno = file->ops.seek(file->inode, fpos))) {
        file->f_pos = fpos;
    }

    unlock_inode(file->inode);

done:
    return DO_STATUS(errno);
}

int
vfs_get_path(struct v_dnode* dnode, char* buf, size_t size, int depth)
{
    if (!dnode) {
        return 0;
    }

    if (depth > 64) {
        return ELOOP;
    }

    size_t len = vfs_get_path(dnode->parent, buf, size, depth + 1);

    if (len >= size) {
        return len;
    }

    size_t cpy_size = MIN(dnode->name.len, size - len);
    strncpy(buf + len, dnode->name.value, cpy_size);
    len += cpy_size;

    if (len < size) {
        buf[len++] = PATH_DELIM;
    }

    return len;
}

int
vfs_readlink(struct v_dnode* dnode, char* buf, size_t size)
{
    const char* link;
    struct v_inode* inode = dnode->inode;
    if (inode->ops.read_symlink) {
        lock_inode(inode);

        int errno = inode->ops.read_symlink(inode, &link);
        strncpy(buf, link, size);

        unlock_inode(inode);
        return errno;
    }
    return 0;
}

__DEFINE_LXSYSCALL3(int, realpathat, int, fd, char*, buf, size_t, size)
{
    int errno;
    struct v_fd* fd_s;
    if ((errno = __vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct v_dnode* dnode;
    errno = vfs_get_path(fd_s->file->dnode, buf, size, 0);

    if (errno >= 0) {
        return errno;
    }

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL3(int, readlink, const char*, path, char*, buf, size_t, size)
{
    int errno;
    struct v_dnode* dnode;
    if (!(errno =
            vfs_walk(__current->cwd, path, &dnode, NULL, VFS_WALK_NOFOLLOW))) {
        errno = vfs_readlink(dnode, buf, size);
    }

    if (errno >= 0) {
        return errno;
    }

    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL4(int,
                    readlinkat,
                    int,
                    dirfd,
                    const char*,
                    pathname,
                    char*,
                    buf,
                    size_t,
                    size)
{
    int errno;
    struct v_fd* fd_s;
    if ((errno = __vfs_getfd(dirfd, &fd_s))) {
        goto done;
    }

    struct v_dnode* dnode;
    if (!(errno = vfs_walk(
            fd_s->file->dnode, pathname, &dnode, NULL, VFS_WALK_NOFOLLOW))) {
        errno = vfs_readlink(fd_s->file->dnode, buf, size);
    }

    if (errno >= 0) {
        return errno;
    }

done:
    return DO_STATUS(errno);
}

/*
    NOTE
    When we perform operation that could affect the layout of
    directory (i.e., rename, mkdir, rmdir). We must lock the parent dir
    whenever possible. This will blocking any ongoing path walking to reach
    it hence avoid any partial state.
*/

__DEFINE_LXSYSCALL1(int, rmdir, const char*, pathname)
{
    int errno;
    struct v_dnode* dnode;
    if ((errno = vfs_walk(__current->cwd, pathname, &dnode, NULL, 0))) {
        return DO_STATUS(errno);
    }

    lock_dnode(dnode);

    if (dnode->parent)
        lock_dnode(dnode->parent);

    if ((dnode->super_block->fs->types & FSTYPE_ROFS)) {
        errno = EROFS;
        goto done;
    }

    if (dnode->ref_count || dnode->inode->open_count) {
        errno = EBUSY;
        goto done;
    }

    if (!llist_empty(&dnode->children)) {
        errno = ENOTEMPTY;
        goto done;
    }

    lock_inode(dnode->inode);

    if ((dnode->inode->itype & VFS_IFDIR)) {
        errno = dnode->inode->ops.rmdir(dnode->inode);
        if (!errno) {
            vfs_dcache_remove(dnode);
            unlock_inode(dnode->inode);
            vfs_d_free(dnode);

            goto done;
        }
    } else {
        errno = ENOTDIR;
    }

    unlock_inode(dnode->inode);

done:
    unlock_dnode(dnode);
    if (dnode->parent)
        unlock_dnode(dnode->parent);
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL1(int, mkdir, const char*, path)
{
    struct v_dnode *parent, *dir = vfs_d_alloc();
    int errno =
      vfs_walk(__current->cwd, path, &parent, &dir->name, VFS_WALK_PARENT);
    if (errno) {
        goto done;
    }

    lock_dnode(parent);
    lock_inode(parent->inode);

    if ((parent->super_block->fs->types & FSTYPE_ROFS)) {
        errno = ENOTSUP;
    } else if (!parent->inode->ops.mkdir) {
        errno = ENOTSUP;
    } else if (!(parent->inode->itype & VFS_IFDIR)) {
        errno = ENOTDIR;
    } else if (!(errno = parent->inode->ops.mkdir(parent->inode, dir))) {
        llist_append(&parent->children, &dir->siblings);
        goto cleanup;
    }

    vfs_d_free(dir);

cleanup:
    unlock_inode(parent->inode);
    unlock_dnode(parent);
done:
    return DO_STATUS(errno);
}

int
__vfs_do_unlink(struct v_dnode* dnode)
{
    struct v_inode* inode = dnode->inode;

    if (dnode->ref_count > 1) {
        return EBUSY;
    }

    lock_inode(inode);

    int errno;
    if (inode->open_count) {
        errno = EBUSY;
    } else if (!(inode->itype & VFS_IFDIR)) {
        // The underlying unlink implementation should handle
        //  symlink case
        errno = inode->ops.unlink(inode);
        if (!errno) {
            inode->link_count--;
            vfs_dcache_remove(dnode);
            vfs_d_free(dnode);
        }
    } else {
        errno = EISDIR;
    }

    unlock_inode(inode);

    return errno;
}

__DEFINE_LXSYSCALL1(int, unlink, const char*, pathname)
{
    int errno;
    struct v_dnode* dnode;
    if ((errno = vfs_walk(__current->cwd, pathname, &dnode, NULL, 0))) {
        goto done;
    }
    if ((dnode->super_block->fs->types & FSTYPE_ROFS)) {
        errno = EROFS;
        goto done;
    }

    errno = __vfs_do_unlink(dnode);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL2(int, unlinkat, int, fd, const char*, pathname)
{
    int errno;
    struct v_fd* fd_s;
    if ((errno = __vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct v_dnode* dnode;
    if (!(errno = vfs_walk(fd_s->file->dnode, pathname, &dnode, NULL, 0))) {
        errno = __vfs_do_unlink(dnode);
    }

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL2(int, link, const char*, oldpath, const char*, newpath)
{
    int errno;
    struct v_dnode *dentry, *to_link, *name_dentry, *name_file;

    errno = __vfs_try_locate_file(oldpath, &dentry, &to_link, 0);
    if (!errno) {
        errno = __vfs_try_locate_file(
          newpath, &name_dentry, &name_file, FLOCATE_CREATE_EMPTY);
        if (!errno) {
            errno = EEXIST;
        } else if (name_file) {
            errno = vfs_link(to_link, name_file);
        }
    }
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL1(int, fsync, int, fildes)
{
    int errno;
    struct v_fd* fd_s;
    if (!(errno = __vfs_getfd(fildes, &fd_s))) {
        errno = vfs_fsync(fd_s->file);
    }

    return DO_STATUS(errno);
}

int
vfs_dup_fd(struct v_fd* old, struct v_fd** new)
{
    int errno = 0;
    struct v_fd* copied = cake_grab(fd_pile);

    memcpy(copied, old, sizeof(struct v_fd));

    atomic_fetch_add(&old->file->ref_count, 1);

    *new = copied;

    return errno;
}

int
vfs_dup2(int oldfd, int newfd)
{
    if (newfd == oldfd) {
        return newfd;
    }

    int errno;
    struct v_fd *oldfd_s, *newfd_s;
    if ((errno = __vfs_getfd(oldfd, &oldfd_s))) {
        goto done;
    }

    if (!TEST_FD(newfd)) {
        errno = EBADF;
        goto done;
    }

    newfd_s = __current->fdtable->fds[newfd];
    if (newfd_s && (errno = vfs_close(newfd_s->file))) {
        goto done;
    }

    if (!(errno = vfs_dup_fd(oldfd_s, &newfd_s))) {
        __current->fdtable->fds[newfd] = newfd_s;
        return newfd;
    }

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL2(int, dup2, int, oldfd, int, newfd)
{
    return vfs_dup2(oldfd, newfd);
}

__DEFINE_LXSYSCALL1(int, dup, int, oldfd)
{
    int errno, newfd;
    struct v_fd *oldfd_s, *newfd_s;
    if ((errno = __vfs_getfd(oldfd, &oldfd_s))) {
        goto done;
    }

    if (!(errno = vfs_alloc_fdslot(&newfd)) &&
        !(errno = vfs_dup_fd(oldfd_s, &newfd_s))) {
        __current->fdtable->fds[newfd] = newfd_s;
        return newfd;
    }

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL2(int,
                    symlink,
                    const char*,
                    pathname,
                    const char*,
                    link_target)
{
    int errno;
    struct v_dnode* dnode;
    if ((errno = vfs_walk(__current->cwd, pathname, &dnode, NULL, 0))) {
        goto done;
    }
    if ((dnode->super_block->fs->types & FSTYPE_ROFS)) {
        errno = EROFS;
        goto done;
    }
    if (!dnode->inode->ops.symlink) {
        errno = ENOTSUP;
        goto done;
    }

    lock_inode(dnode->inode);

    errno = dnode->inode->ops.symlink(dnode->inode, link_target);

    unlock_inode(dnode->inode);

done:
    return DO_STATUS(errno);
}

int
__vfs_do_chdir(struct v_dnode* dnode)
{
    int errno = 0;

    lock_dnode(dnode);

    if (!(dnode->inode->itype & VFS_IFDIR)) {
        errno = ENOTDIR;
        goto done;
    }

    if (__current->cwd) {
        atomic_fetch_add(&__current->cwd->ref_count, 1);
    }

    atomic_fetch_sub(&dnode->ref_count, 1);
    __current->cwd = dnode;

    unlock_dnode(dnode);

done:
    return errno;
}

__DEFINE_LXSYSCALL1(int, chdir, const char*, path)
{
    struct v_dnode* dnode;
    int errno = 0;

    if ((errno = vfs_walk(__current->cwd, path, &dnode, NULL, 0))) {
        goto done;
    }

    errno = __vfs_do_chdir(dnode);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL1(int, fchdir, int, fd)
{
    struct v_fd* fd_s;
    int errno = 0;

    if ((errno = __vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    errno = __vfs_do_chdir(fd_s->file->dnode);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL2(char*, getcwd, char*, buf, size_t, size)
{
    int errno = 0;
    char* ret_ptr = 0;
    if (size < 2) {
        errno = ERANGE;
        goto done;
    }

    size_t len = 0;

    if (!__current->cwd) {
        *buf = PATH_DELIM;
        len = 1;
    } else {
        len = vfs_get_path(__current->cwd, buf, size, 0);
        if (len == size) {
            errno = ERANGE;
            goto done;
        }
    }

    buf[len + 1] = '\0';

    ret_ptr = buf;

done:
    __current->k_status = errno;
    return ret_ptr;
}

int
vfs_do_rename(struct v_dnode* current, struct v_dnode* target)
{
    if (current->inode->id == target->inode->id) {
        // hard link
        return 0;
    }

    if (current->ref_count > 1 || target->ref_count > 1) {
        return EBUSY;
    }

    if (current->super_block != target->super_block) {
        return EXDEV;
    }

    int errno = 0;

    struct v_dnode* oldparent = current->parent;
    struct v_dnode* newparent = target->parent;

    lock_dnode(current);
    lock_dnode(target);
    if (oldparent)
        lock_dnode(oldparent);
    if (newparent)
        lock_dnode(newparent);

    if (!llist_empty(&target->children)) {
        errno = ENOTEMPTY;
        unlock_dnode(target);
        goto cleanup;
    }

    if ((errno = current->inode->ops.rename(current->inode, current, target))) {
        unlock_dnode(target);
        goto cleanup;
    }

    // re-position current
    hstrcpy(&current->name, &target->name);
    vfs_dcache_rehash(newparent, current);

    // detach target
    vfs_dcache_remove(target);

    unlock_dnode(target);

cleanup:
    unlock_dnode(current);
    if (oldparent)
        unlock_dnode(oldparent);
    if (newparent)
        unlock_dnode(newparent);

    return errno;
}

__DEFINE_LXSYSCALL2(int, rename, const char*, oldpath, const char*, newpath)
{
    struct v_dnode *cur, *target_parent, *target;
    struct hstr name = HSTR(valloc(VFS_NAME_MAXLEN), 0);
    int errno = 0;

    if ((errno = vfs_walk(__current->cwd, oldpath, &cur, NULL, 0))) {
        goto done;
    }

    if ((errno = vfs_walk(
           __current->cwd, newpath, &target_parent, &name, VFS_WALK_PARENT))) {
        goto done;
    }

    errno = vfs_walk(target_parent, name.value, &target, NULL, 0);
    if (errno == ENOENT) {
        target = vfs_d_alloc();
        hstrcpy(&target->name, &name);
    } else if (errno) {
        goto done;
    }

    if (!(errno = vfs_do_rename(cur, target))) {
        vfs_d_free(target);
    }

done:
    vfree(name.value);
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL3(int,
                    mount,
                    const char*,
                    source,
                    const char*,
                    target,
                    const char*,
                    fstype)
{
    struct v_dnode *dev, *mnt;
    int errno = 0;

    if ((errno = vfs_walk(__current->cwd, source, &dev, NULL, 0))) {
        goto done;
    }

    if ((errno = vfs_walk(__current->cwd, target, &mnt, NULL, 0))) {
        goto done;
    }

    if (!(dev->inode->itype & VFS_IFVOLDEV)) {
        errno = ENOTDEV;
        goto done;
    }

    if (mnt->ref_count > 1) {
        errno = EBUSY;
        goto done;
    }

    // FIXME should not touch the underlying fs!
    struct device* device =
      (struct device*)((struct twifs_node*)dev->inode->data)->data;

    errno = vfs_mount_at(fstype, device, mnt);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL1(int, unmount, const char*, target)
{
    return vfs_unmount(target);
}