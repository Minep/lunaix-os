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

 1. Get inodes hooked into lru (CHECKED)
 2. Get dnodes hooked into lru (CHECKED)
 3. Get inodes properly hashed so they can be reused by underling fs (CHECKED)
 4. (lru) Add a callback function (or destructor) for eviction. (CHECKED)
        [good idea] or a constructor/destructor pattern in cake allocator ?
 5. (mount) Figure out a way to identify a busy mount point before unmount
            maybe a unified mount_point structure that maintain a referencing
            counter on any dnodes within the subtree? Such a counter will only
            increament if a file is opened or a dnode is being used as working
            directory and decreamenting conversely. (CHECKED)
 6. (mount) Ability to track all mount points (including sub-mounts)
            so we can be confident to clean up everything when we
            unmount. (CHECKED)
 7. (mount) Figure out a way to acquire the device represented by a dnode.
            so it can be used to mount. (e.g. we wish to get `struct device*`
            out of the dnode at /dev/sda)
            [tip] we should pay attention at twifs and add a private_data field
            under struct v_dnode? (CHECKED)
 8. (mount) Then, we should refactor on mount/unmount mechanism. (CHECKED)
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

static struct cake_pile* dnode_pile;
static struct cake_pile* inode_pile;
static struct cake_pile* file_pile;
static struct cake_pile* superblock_pile;
static struct cake_pile* fd_pile;

struct v_dnode* vfs_sysroot;
static struct hbucket* dnode_cache;

struct lru_zone *dnode_lru, *inode_lru;

struct hstr vfs_ddot = HSTR("..", 2);
struct hstr vfs_dot = HSTR(".", 1);
struct hstr vfs_empty = HSTR("", 0);

struct v_superblock*
vfs_sb_alloc();

void
vfs_sb_free(struct v_superblock* sb);

static int
__vfs_try_evict_dnode(struct lru_node* obj);

static int
__vfs_try_evict_inode(struct lru_node* obj);

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

    dnode_cache = vzalloc(VFS_HASHTABLE_SIZE * sizeof(struct hbucket));

    dnode_lru = lru_new_zone(__vfs_try_evict_dnode);
    inode_lru = lru_new_zone(__vfs_try_evict_inode);

    hstr_rehash(&vfs_ddot, HSTR_FULL_HASH);
    hstr_rehash(&vfs_dot, HSTR_FULL_HASH);

    // 创建一个根dnode。
    vfs_sysroot = vfs_d_alloc(NULL, &vfs_empty);
    vfs_sysroot->parent = vfs_sysroot;
    atomic_fetch_add(&vfs_sysroot->ref_count, 1);
}

inline struct hbucket*
__dcache_hash(struct v_dnode* parent, u32_t* hash)
{
    u32_t _hash = *hash;
    // 确保低位更加随机
    _hash = _hash ^ (_hash >> VFS_HASHBITS);
    // 与parent的指针值做加法，来减小碰撞的可能性。
    _hash += (u32_t)parent;
    *hash = _hash;
    return &dnode_cache[_hash & VFS_HASH_MASK];
}

struct v_dnode*
vfs_dcache_lookup(struct v_dnode* parent, struct hstr* str)
{
    if (!str->len || HSTR_EQ(str, &vfs_dot))
        return parent;

    if (HSTR_EQ(str, &vfs_ddot)) {
        return parent->parent;
    }

    u32_t hash = str->hash;
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
    assert(parent);

    atomic_fetch_add(&dnode->ref_count, 1);
    dnode->parent = parent;
    llist_append(&parent->children, &dnode->siblings);

    struct hbucket* bucket = __dcache_hash(parent, &dnode->name.hash);
    hlist_add(&bucket->head, &dnode->hash_list);
}

void
vfs_dcache_remove(struct v_dnode* dnode)
{
    assert(dnode);
    assert(dnode->ref_count == 1);

    llist_delete(&dnode->siblings);
    llist_delete(&dnode->aka_list);
    hlist_delete(&dnode->hash_list);

    dnode->parent = NULL;
    atomic_fetch_sub(&dnode->ref_count, 1);
}

void
vfs_dcache_rehash(struct v_dnode* new_parent, struct v_dnode* dnode)
{
    assert(new_parent);

    hstr_rehash(&dnode->name, HSTR_FULL_HASH);
    vfs_dcache_remove(dnode);
    vfs_dcache_add(new_parent, dnode);
}

int
vfs_open(struct v_dnode* dnode, struct v_file** file)
{
    if (!dnode->inode || !dnode->inode->ops->open) {
        return ENOTSUP;
    }

    struct v_inode* inode = dnode->inode;

    lock_inode(inode);

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

    int errno = inode->ops->open(inode, vfile);
    if (errno) {
        cake_release(file_pile, vfile);
    } else {
        atomic_fetch_add(&dnode->ref_count, 1);
        inode->open_count++;
        mnt_mkbusy(dnode->mnt);

        *file = vfile;
    }

    unlock_inode(inode);

    return errno;
}

void
vfs_assign_inode(struct v_dnode* assign_to, struct v_inode* inode)
{
    if (assign_to->inode) {
        llist_delete(&assign_to->aka_list);
        assign_to->inode->link_count--;
    }
    llist_append(&inode->aka_dnodes, &assign_to->aka_list);
    assign_to->inode = inode;
    inode->link_count++;
}

int
vfs_link(struct v_dnode* to_link, struct v_dnode* name)
{
    int errno;

    if ((errno = vfs_check_writable(to_link))) {
        return errno;
    }

    lock_inode(to_link->inode);
    if (to_link->super_block->root != name->super_block->root) {
        errno = EXDEV;
    } else if (!to_link->inode->ops->link) {
        errno = ENOTSUP;
    } else if (!(errno = to_link->inode->ops->link(to_link->inode, name))) {
        vfs_assign_inode(name, to_link->inode);
    }
    unlock_inode(to_link->inode);

    return errno;
}

int
vfs_pclose(struct v_file* file, pid_t pid)
{
    int errno = 0;
    if (file->ref_count > 1) {
        atomic_fetch_sub(&file->ref_count, 1);
    } else if (!(errno = file->ops->close(file))) {
        atomic_fetch_sub(&file->dnode->ref_count, 1);
        file->inode->open_count--;

        /*
         * Prevent dead lock.
         * This happened when process is terminated while blocking on read.
         * In that case, the process is still holding the inode lock and it
             will never get released.
         * The unlocking should also include ownership check.
         *
         * To see why, consider two process both open the same file both with
         * fd=x.
         *      Process A: busy on reading x
         *      Process B: do nothing with x
         * Assuming that, after a very short time, process B get terminated
         * while process A is still busy in it's reading business. By this
         * design, the inode lock of this file x is get released by B rather
         * than A. And this will cause a probable race condition on A if other
         * process is writing to this file later after B exit.
         */
        if (mutex_on_hold(&file->inode->lock)) {
            mutex_unlock_for(&file->inode->lock, pid);
        }
        mnt_chillax(file->dnode->mnt);

        pcache_commit_all(file->inode);
        cake_release(file_pile, file);
    }
    return errno;
}

int
vfs_close(struct v_file* file)
{
    return vfs_pclose(file, __current->pid);
}

void
vfs_free_fd(struct v_fd* fd)
{
    cake_release(fd_pile, fd);
}

int
vfs_fsync(struct v_file* file)
{
    int errno;
    if ((errno = vfs_check_writable(file->dnode))) {
        return errno;
    }

    lock_inode(file->inode);

    pcache_commit_all(file->inode);

    errno = ENOTSUP;
    if (file->ops->sync) {
        errno = file->ops->sync(file);
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
    sb->i_cache = vzalloc(VFS_HASHTABLE_SIZE * sizeof(struct hbucket));
    return sb;
}

void
vfs_sb_free(struct v_superblock* sb)
{
    vfree(sb->i_cache);
    cake_release(superblock_pile, sb);
}

static int
__vfs_try_evict_dnode(struct lru_node* obj)
{
    struct v_dnode* dnode = container_of(obj, struct v_dnode, lru);

    if (!dnode->ref_count) {
        vfs_d_free(dnode);
        return 1;
    }
    return 0;
}

static int
__vfs_try_evict_inode(struct lru_node* obj)
{
    struct v_inode* inode = container_of(obj, struct v_inode, lru);

    if (!inode->link_count && !inode->open_count) {
        vfs_i_free(inode);
        return 1;
    }
    return 0;
}

struct v_dnode*
vfs_d_alloc(struct v_dnode* parent, struct hstr* name)
{
    struct v_dnode* dnode = cake_grab(dnode_pile);
    if (!dnode) {
        lru_evict_half(dnode_lru);

        if (!(dnode = cake_grab(dnode_pile))) {
            return NULL;
        }
    }

    memset(dnode, 0, sizeof(*dnode));
    llist_init_head(&dnode->children);
    llist_init_head(&dnode->siblings);
    llist_init_head(&dnode->aka_list);
    mutex_init(&dnode->lock);

    dnode->ref_count = ATOMIC_VAR_INIT(0);
    dnode->name = HHSTR(vzalloc(VFS_NAME_MAXLEN), 0, 0);

    hstrcpy(&dnode->name, name);

    if (parent) {
        dnode->super_block = parent->super_block;
        dnode->mnt = parent->mnt;
    }

    lru_use_one(dnode_lru, &dnode->lru);

    return dnode;
}

void
vfs_d_free(struct v_dnode* dnode)
{
    assert(dnode->ref_count == 1);

    if (dnode->inode) {
        assert(dnode->inode->link_count > 0);
        dnode->inode->link_count--;
    }

    vfs_dcache_remove(dnode);
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
vfs_i_find(struct v_superblock* sb, u32_t i_id)
{
    struct hbucket* slot = &sb->i_cache[i_id & VFS_HASH_MASK];
    struct v_inode *pos, *n;
    hashtable_bucket_foreach(slot, pos, n, hash_list)
    {
        if (pos->id == i_id) {
            lru_use_one(inode_lru, &pos->lru);
            return pos;
        }
    }

    return NULL;
}

void
vfs_i_addhash(struct v_inode* inode)
{
    struct hbucket* slot = &inode->sb->i_cache[inode->id & VFS_HASH_MASK];

    hlist_delete(&inode->hash_list);
    hlist_add(&slot->head, &inode->hash_list);
}

struct v_inode*
vfs_i_alloc(struct v_superblock* sb)
{
    assert(sb->ops.init_inode);

    struct v_inode* inode;
    if (!(inode = cake_grab(inode_pile))) {
        lru_evict_half(inode_lru);
        if (!(inode = cake_grab(inode_pile))) {
            return NULL;
        }
    }

    memset(inode, 0, sizeof(*inode));
    mutex_init(&inode->lock);
    llist_init_head(&inode->xattrs);
    llist_init_head(&inode->aka_dnodes);

    sb->ops.init_inode(sb, inode);

    inode->sb = sb;
    inode->ctime = clock_unixtime();
    inode->atime = inode->ctime;
    inode->mtime = inode->ctime;

done:
    lru_use_one(inode_lru, &inode->lru);
    return inode;
}

void
vfs_i_free(struct v_inode* inode)
{
    if (inode->pg_cache) {
        pcache_release(inode->pg_cache);
        vfree(inode->pg_cache);
    }
    // we don't need to sync inode.
    // If an inode can be free, then it must be properly closed.
    // Hence it must be synced already!
    if (inode->destruct) {
        inode->destruct(inode);
    }
    hlist_delete(&inode->hash_list);
    cake_release(inode_pile, inode);
}

/* ---- System call definition and support ---- */

#define FLOCATE_CREATE_EMPTY 1

int
vfs_getfd(int fd, struct v_fd** fd_s)
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

    name_str[0] = 0;
    if ((errno = vfs_walk_proc(path, fdir, &name, VFS_WALK_PARENT))) {
        return errno;
    }

    errno = vfs_walk(*fdir, name.value, file, NULL, 0);
    if (errno != ENOENT || !(options & FLOCATE_CREATE_EMPTY)) {
        return errno;
    }

    struct v_dnode* parent = *fdir;
    struct v_dnode* file_new = vfs_d_alloc(parent, &name);

    if (!file_new) {
        return ENOMEM;
    }

    lock_dnode(parent);

    if (!(errno = parent->inode->ops->create(parent->inode, file_new))) {
        vfs_dcache_add(parent, file_new);
        *file = file_new;
    } else {
        vfs_d_free(file_new);
    }

    unlock_dnode(parent);

    return errno;
}

int
vfs_do_open(const char* path, int options)
{
    int errno, fd;
    struct v_dnode *dentry, *file;
    struct v_file* ofile = NULL;

    errno = __vfs_try_locate_file(
      path, &dentry, &file, (options & FO_CREATE) ? FLOCATE_CREATE_EMPTY : 0);

    if (!errno && !(errno = vfs_alloc_fdslot(&fd))) {

        if (errno || (errno = vfs_open(file, &ofile))) {
            return errno;
        }

        struct v_fd* fd_s = cake_grab(fd_pile);
        memset(fd_s, 0, sizeof(*fd_s));

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
    if ((errno = vfs_getfd(fd, &fd_s))) {
        goto done_err;
    }

    if ((errno = vfs_close(fd_s->file))) {
        goto done_err;
    }

    cake_release(fd_pile, fd_s);
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

    if ((errno = vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct v_inode* inode = fd_s->file->inode;

    lock_inode(inode);

    if (!(inode->itype & VFS_IFDIR)) {
        errno = ENOTDIR;
    } else {
        struct dir_context dctx =
          (struct dir_context){ .cb_data = dent,
                                .index = dent->d_offset,
                                .read_complete_callback =
                                  __vfs_readdir_callback };
        errno = 1;
        if (dent->d_offset == 0) {
            __vfs_readdir_callback(&dctx, vfs_dot.value, vfs_dot.len, DT_DIR);
        } else if (dent->d_offset == 1) {
            __vfs_readdir_callback(&dctx, vfs_ddot.value, vfs_ddot.len, DT_DIR);
        } else {
            dctx.index -= 2;
            if ((errno = fd_s->file->ops->readdir(fd_s->file, &dctx)) != 1) {
                unlock_inode(inode);
                goto done;
            }
        }
        dent->d_offset++;
    }

    unlock_inode(inode);

done:
    return DO_STATUS_OR_RETURN(errno);
}

__DEFINE_LXSYSCALL3(int, read, int, fd, void*, buf, size_t, count)
{
    int errno = 0;
    struct v_fd* fd_s;
    if ((errno = vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct v_file* file = fd_s->file;
    if ((file->inode->itype & VFS_IFDIR)) {
        errno = EISDIR;
        goto done;
    }

    lock_inode(file->inode);

    file->inode->atime = clock_unixtime();

    if ((file->inode->itype & VFS_IFSEQDEV) || (fd_s->flags & FO_DIRECT)) {
        errno = file->ops->read(file->inode, buf, count, file->f_pos);
    } else {
        errno = pcache_read(file->inode, buf, count, file->f_pos);
    }

    if (errno > 0) {
        file->f_pos += errno;
        unlock_inode(file->inode);
        return errno;
    }

    unlock_inode(file->inode);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL3(int, write, int, fd, void*, buf, size_t, count)
{
    int errno = 0;
    struct v_fd* fd_s;
    if ((errno = vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct v_file* file = fd_s->file;

    if ((errno = vfs_check_writable(file->dnode))) {
        goto done;
    }

    if ((file->inode->itype & VFS_IFDIR)) {
        errno = EISDIR;
        goto done;
    }

    lock_inode(file->inode);

    file->inode->mtime = clock_unixtime();

    if ((file->inode->itype & VFS_IFSEQDEV) || (fd_s->flags & FO_DIRECT)) {
        errno = file->ops->write(file->inode, buf, count, file->f_pos);
    } else {
        errno = pcache_write(file->inode, buf, count, file->f_pos);
    }

    if (errno > 0) {
        file->f_pos += errno;
        unlock_inode(file->inode);
        return errno;
    }

    unlock_inode(file->inode);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL3(int, lseek, int, fd, int, offset, int, options)
{
    int errno = 0;
    struct v_fd* fd_s;
    if ((errno = vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct v_file* file = fd_s->file;

    if (!file->ops->seek) {
        errno = ENOTSUP;
        goto done;
    }

    lock_inode(file->inode);

    int overflow = 0;
    int fpos = file->f_pos;
    switch (options) {
        case FSEEK_CUR:
            overflow = __builtin_sadd_overflow((int)file->f_pos, offset, &fpos);
            break;
        case FSEEK_END:
            overflow =
              __builtin_sadd_overflow((int)file->inode->fsize, offset, &fpos);
            break;
        case FSEEK_SET:
            fpos = offset;
            break;
    }
    if (overflow) {
        errno = EOVERFLOW;
    } else if (!(errno = file->ops->seek(file->inode, fpos))) {
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
        return ENAMETOOLONG;
    }

    size_t len = 0;

    if (dnode->parent != dnode) {
        len = vfs_get_path(dnode->parent, buf, size, depth + 1);
    }

    if (len >= size) {
        return len;
    }

    if (!len || buf[len - 1] != VFS_PATH_DELIM) {
        buf[len++] = VFS_PATH_DELIM;
    }

    size_t cpy_size = MIN(dnode->name.len, size - len);
    strncpy(buf + len, dnode->name.value, cpy_size);
    len += cpy_size;

    return len;
}

int
vfs_readlink(struct v_dnode* dnode, char* buf, size_t size)
{
    const char* link;
    struct v_inode* inode = dnode->inode;
    if (inode->ops->read_symlink) {
        lock_inode(inode);

        int errno = inode->ops->read_symlink(inode, &link);
        strncpy(buf, link, size);

        unlock_inode(inode);
        return errno;
    }
    return 0;
}

int
vfs_get_dtype(int itype)
{
    switch (itype) {
        case VFS_IFDIR:
            return DT_DIR;
        case VFS_IFSYMLINK:
            return DT_SYMLINK;
        default:
            return DT_PIPE;
    }
}

__DEFINE_LXSYSCALL3(int, realpathat, int, fd, char*, buf, size_t, size)
{
    int errno;
    struct v_fd* fd_s;
    if ((errno = vfs_getfd(fd, &fd_s))) {
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
    if (!(errno = vfs_walk_proc(path, &dnode, NULL, VFS_WALK_NOFOLLOW))) {
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
    if ((errno = vfs_getfd(dirfd, &fd_s))) {
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
    if ((errno = vfs_walk_proc(pathname, &dnode, NULL, 0))) {
        return DO_STATUS(errno);
    }

    lock_dnode(dnode);

    if ((errno = vfs_check_writable(dnode))) {
        goto done;
    }

    if ((dnode->super_block->fs->types & FSTYPE_ROFS)) {
        errno = EROFS;
        goto done;
    }

    if (dnode->ref_count > 1 || dnode->inode->open_count) {
        errno = EBUSY;
        goto done;
    }

    if (!llist_empty(&dnode->children)) {
        errno = ENOTEMPTY;
        goto done;
    }

    struct v_dnode* parent = dnode->parent;

    if (!parent) {
        errno = EINVAL;
        goto done;
    }

    lock_dnode(parent);
    lock_inode(parent->inode);

    if ((dnode->inode->itype & VFS_IFDIR)) {
        errno = parent->inode->ops->rmdir(parent->inode, dnode);
        if (!errno) {
            vfs_dcache_remove(dnode);
        }
    } else {
        errno = ENOTDIR;
    }

    unlock_inode(parent->inode);
    unlock_dnode(parent);

done:
    unlock_dnode(dnode);
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL1(int, mkdir, const char*, path)
{
    int errno = 0;
    struct v_dnode *parent, *dir;
    char name_value[VFS_NAME_MAXLEN];
    struct hstr name = HHSTR(name_value, 0, 0);

    if ((errno = vfs_walk_proc(path, &parent, &name, VFS_WALK_PARENT))) {
        goto done;
    }

    if ((errno = vfs_check_writable(parent))) {
        goto done;
    }

    if (!(dir = vfs_d_alloc(parent, &name))) {
        errno = ENOMEM;
        goto done;
    }

    lock_dnode(parent);
    lock_inode(parent->inode);

    if ((parent->super_block->fs->types & FSTYPE_ROFS)) {
        errno = ENOTSUP;
    } else if (!parent->inode->ops->mkdir) {
        errno = ENOTSUP;
    } else if (!(parent->inode->itype & VFS_IFDIR)) {
        errno = ENOTDIR;
    } else if (!(errno = parent->inode->ops->mkdir(parent->inode, dir))) {
        vfs_dcache_add(parent, dir);
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
    int errno;
    struct v_inode* inode = dnode->inode;

    if (dnode->ref_count > 1) {
        return EBUSY;
    }

    if ((errno = vfs_check_writable(dnode))) {
        return errno;
    }

    lock_inode(inode);

    if (inode->open_count) {
        errno = EBUSY;
    } else if (!(inode->itype & VFS_IFDIR)) {
        // The underlying unlink implementation should handle
        //  symlink case
        errno = inode->ops->unlink(inode);
        if (!errno) {
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
    if ((errno = vfs_walk_proc(pathname, &dnode, NULL, 0))) {
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
    if ((errno = vfs_getfd(fd, &fd_s))) {
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

    if (!(errno = vfs_getfd(fildes, &fd_s))) {
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
    if ((errno = vfs_getfd(oldfd, &oldfd_s))) {
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
    if ((errno = vfs_getfd(oldfd, &oldfd_s))) {
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
    if ((errno = vfs_walk_proc(pathname, &dnode, NULL, 0))) {
        goto done;
    }

    if (errno = vfs_check_writable(dnode)) {
        goto done;
    }

    if (!dnode->inode->ops->set_symlink) {
        errno = ENOTSUP;
        goto done;
    }

    lock_inode(dnode->inode);

    errno = dnode->inode->ops->set_symlink(dnode->inode, link_target);

    unlock_inode(dnode->inode);

done:
    return DO_STATUS(errno);
}

void
vfs_ref_dnode(struct v_dnode* dnode)
{
    atomic_fetch_add(&dnode->ref_count, 1);
    mnt_mkbusy(dnode->mnt);
}

void
vfs_unref_dnode(struct v_dnode* dnode)
{
    atomic_fetch_sub(&dnode->ref_count, 1);
    mnt_chillax(dnode->mnt);
}

int
vfs_do_chdir(struct proc_info* proc, struct v_dnode* dnode)
{
    int errno = 0;

    lock_dnode(dnode);

    if (!(dnode->inode->itype & VFS_IFDIR)) {
        errno = ENOTDIR;
        goto done;
    }

    if (proc->cwd) {
        vfs_unref_dnode(proc->cwd);
    }

    vfs_ref_dnode(dnode);
    proc->cwd = dnode;

    unlock_dnode(dnode);

done:
    return errno;
}

__DEFINE_LXSYSCALL1(int, chdir, const char*, path)
{
    struct v_dnode* dnode;
    int errno = 0;

    if ((errno = vfs_walk_proc(path, &dnode, NULL, 0))) {
        goto done;
    }

    errno = vfs_do_chdir(__current, dnode);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL1(int, fchdir, int, fd)
{
    struct v_fd* fd_s;
    int errno = 0;

    if ((errno = vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    errno = vfs_do_chdir(__current, fd_s->file->dnode);

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
        *buf = VFS_PATH_DELIM;
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
    int errno = 0;
    if (current->inode->id == target->inode->id) {
        // hard link
        return 0;
    }

    if (errno = vfs_check_writable(current)) {
        return errno;
    }

    if (current->ref_count > 1 || target->ref_count > 1) {
        return EBUSY;
    }

    if (current->super_block != target->super_block) {
        return EXDEV;
    }

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

    if ((errno =
           current->inode->ops->rename(current->inode, current, target))) {
        unlock_dnode(target);
        goto cleanup;
    }

    // re-position current
    hstrcpy(&current->name, &target->name);
    vfs_dcache_rehash(newparent, current);

    // detach target
    vfs_d_free(target);

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

    if ((errno = vfs_walk_proc(oldpath, &cur, NULL, 0))) {
        goto done;
    }

    if ((errno = vfs_walk(
           __current->cwd, newpath, &target_parent, &name, VFS_WALK_PARENT))) {
        goto done;
    }

    errno = vfs_walk(target_parent, name.value, &target, NULL, 0);
    if (errno == ENOENT) {
        target = vfs_d_alloc(target_parent, &name);
        vfs_dcache_add(target_parent, target);
    } else if (errno) {
        goto done;
    }

    if (!target) {
        errno = ENOMEM;
        goto done;
    }

    errno = vfs_do_rename(cur, target);

done:
    vfree(name.value);
    return DO_STATUS(errno);
}