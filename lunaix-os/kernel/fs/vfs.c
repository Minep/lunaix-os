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
            [good idea] <class>:<subclass>:<uniq_id> composition (CHECKED)
*/

#include <klibc/string.h>
#include <lunaix/foptions.h>
#include <lunaix/fs.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

#include <lunaix/fs/twifs.h>

#include <usr/lunaix/dirent_defs.h>

#define INODE_ACCESSED  0
#define INODE_MODIFY    1

static struct cake_pile* dnode_pile;
static struct cake_pile* inode_pile;
static struct cake_pile* file_pile;
static struct cake_pile* superblock_pile;
static struct cake_pile* fd_pile;

struct v_dnode* vfs_sysroot = NULL;

struct lru_zone *dnode_lru, *inode_lru;

struct hstr vfs_ddot = HSTR("..", 2);
struct hstr vfs_dot = HSTR(".", 1);
struct hstr vfs_empty = HSTR("", 0);

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

    dnode_lru = lru_new_zone("vfs_dnode", __vfs_try_evict_dnode);
    inode_lru = lru_new_zone("vfs_inode", __vfs_try_evict_inode);

    hstr_rehash(&vfs_ddot, HSTR_FULL_HASH);
    hstr_rehash(&vfs_dot, HSTR_FULL_HASH);

    // 创建一个根dnode。
    vfs_sysroot = vfs_d_alloc(NULL, &vfs_empty);
    vfs_sysroot->parent = vfs_sysroot;
    
    vfs_ref_dnode(vfs_sysroot);
}

static inline struct hbucket*
__dcache_hash(struct v_dnode* parent, u32_t* hash)
{
    struct hbucket* d_cache;
    u32_t _hash;
    
    d_cache = parent->super_block->d_cache;
    _hash = *hash;
    _hash = _hash ^ (_hash >> VFS_HASHBITS);
    _hash += (u32_t)__ptr(parent);

    *hash = _hash;
    return &d_cache[_hash & VFS_HASH_MASK];
}

static inline int
__sync_inode_nolock(struct v_inode* inode)
{
    pcache_commit_all(inode);

    int errno = ENOTSUP;
    if (inode->ops->sync) {
        errno = inode->ops->sync(inode);
    }

    return errno;
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
        if (pos->name.hash == hash && pos->parent == parent) {
            return pos;
        }
    }
    return NULL;
}

static void
__vfs_touch_inode(struct v_inode* inode, const int type)
{
    if (type == INODE_MODIFY) {
        inode->mtime = clock_unixtime();
    }

    else if (type == INODE_ACCESSED) {
        inode->atime = clock_unixtime();
    }

    lru_use_one(inode_lru, &inode->lru);
}

void
vfs_dcache_add(struct v_dnode* parent, struct v_dnode* dnode)
{
    assert(parent);

    dnode->ref_count = 1;
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
    dnode->ref_count = 0;
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
    struct v_inode* inode = dnode->inode;
    
    if (!inode || !inode->ops->open) {
        return ENOTSUP;
    }

    lock_inode(inode);

    struct v_file* vfile = cake_grab(file_pile);
    memset(vfile, 0, sizeof(*vfile));

    vfile->dnode = dnode;
    vfile->inode = inode;
    vfile->ref_count = 1;
    vfile->ops = inode->default_fops;

    if (check_regfile_node(inode) && !inode->pg_cache) {
        struct pcache* pcache = vzalloc(sizeof(struct pcache));
        pcache_init(pcache);
        pcache->master = inode;
        inode->pg_cache = pcache;
    }

    int errno = inode->ops->open(inode, vfile);
    if (errno) {
        cake_release(file_pile, vfile);
    } else {
        vfs_ref_dnode(dnode);
        inode->open_count++;

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
    struct v_inode* inode;
    int errno = 0;

    inode = file->inode;

    if (vfs_check_duped_file(file)) {
        vfs_unref_file(file);
        return 0;
    }

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
    mutex_unlock_for(&inode->lock, pid);

    // now regain lock for inode syncing

    lock_inode(inode);

    if ((errno = file->ops->close(file))) {
        goto done;
    }

    vfs_unref_dnode(file->dnode);
    cake_release(file_pile, file);

    pcache_commit_all(inode);
    inode->open_count--;

    if (!inode->open_count) {
        __sync_inode_nolock(inode);
    }

done:
    unlock_inode(inode);
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
vfs_isync(struct v_inode* inode)
{
    lock_inode(inode);

    int errno = __sync_inode_nolock(inode);

    unlock_inode(inode);

    return errno;
}

int
vfs_fsync(struct v_file* file)
{
    int errno;
    if ((errno = vfs_check_writable(file->dnode))) {
        return errno;
    }

    return vfs_isync(file->inode);
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
    sb->d_cache = vzalloc(VFS_HASHTABLE_SIZE * sizeof(struct hbucket));

    sb->ref_count = 1;
    return sb;
}

void
vfs_sb_ref(struct v_superblock* sb)
{
    sb->ref_count++;
}

void
vfs_sb_unref(struct v_superblock* sb)
{
    assert(sb->ref_count);

    sb->ref_count--;
    if (likely(sb->ref_count)) {
        return;
    }

    if (sb->ops.release) {
        sb->ops.release(sb);
    }

    vfree(sb->i_cache);
    vfree(sb->d_cache);
    
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

    dnode->name = HHSTR(vzalloc(VFS_NAME_MAXLEN), 0, 0);

    hstrcpy(&dnode->name, name);

    if (parent) {
        vfs_d_assign_sb(dnode, parent->super_block);
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

    if (dnode->destruct) {
        dnode->destruct(dnode);
    }

    vfs_sb_unref(dnode->super_block);
    vfree((void*)dnode->name.value);
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

    inode->ctime = clock_unixtime();
    inode->atime = inode->ctime;
    inode->mtime = inode->ctime;

    vfs_i_assign_sb(inode, sb);
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

    vfs_sb_unref(inode->sb);
    hlist_delete(&inode->hash_list);
    cake_release(inode_pile, inode);
}

/* ---- System call definition and support ---- */

// make a new name when not exists
#define FLOC_MAYBE_MKNAME 1

// name must be non-exist and made.
#define FLOC_MKNAME 2

// no follow symlink
#define FLOC_NOFOLLOW 4

int
vfs_getfd(int fd, struct v_fd** fd_s)
{
    if (TEST_FD(fd) && (*fd_s = __current->fdtable->fds[fd])) {
        return 0;
    }
    return EBADF;
}

static int
__vfs_mknod(struct v_inode* parent, struct v_dnode* dnode, 
            unsigned int itype, dev_t* dev)
{
    int errno;

    errno = parent->ops->create(parent, dnode, itype);
    if (errno) {
        return errno;
    }

    return 0;
}

struct file_locator {
    struct v_dnode* dir;
    struct v_dnode* file;
    bool fresh;
};

/**
 * @brief unlock the file locator (floc) if possible.
 *        If the file to be located if not exists, and
 *        any FLOC_*MKNAME flag is set, then the parent
 *        dnode will be locked until the file has been properly
 *        finalised by subsequent logic.
 * 
 * @param floc 
 */
static inline void
__floc_try_unlock(struct file_locator* floc)
{
    if (floc->fresh) {
        assert(floc->dir);
        unlock_dnode(floc->dir);
    }
}

static int
__vfs_try_locate_file(const char* path,
                      struct file_locator* floc,
                      int options)
{
    char name_str[VFS_NAME_MAXLEN];
    struct v_dnode *fdir, *file;
    struct hstr name = HSTR(name_str, 0);
    int errno, woption = 0;

    if ((options & FLOC_NOFOLLOW)) {
        woption |= VFS_WALK_NOFOLLOW;
        options &= ~FLOC_NOFOLLOW;
    }

    floc->fresh = false;
    name_str[0] = 0;
    errno = vfs_walk_proc(path, &fdir, &name, woption | VFS_WALK_PARENT);
    if (errno) {
        return errno;
    }

    lock_dnode(fdir);

    errno = vfs_walk(fdir, name.value, &file, NULL, woption);

    if (errno && errno != ENOENT) {
        goto error;
    }

    if (!errno && (options & FLOC_MKNAME)) {
        errno = EEXIST;
        goto error;
    }
    
    if (!errno) {
        // the file present, no need to hold the directory lock
        unlock_dnode(fdir);
        goto done;
    }

    // errno == ENOENT
    if (!options) {
        goto error;
    }

    errno = vfs_check_writable(fdir);
    if (errno) {
        goto error;
    }

    floc->fresh = true;

    file = vfs_d_alloc(fdir, &name);

    if (!file) {
        errno = ENOMEM;
        goto error;
    }

    vfs_dcache_add(fdir, file);

done:
    floc->dir   = fdir;
    floc->file  = file;
    
    return errno;

error:
    unlock_dnode(fdir);
    return errno;
}


static bool
__check_unlinkable(struct v_dnode* dnode)
{
    int acl;
    bool wr_self, wr_parent;
    struct v_dnode* parent;

    parent = dnode->parent;
    acl = dnode->inode->acl;

    wr_self = check_allow_write(dnode->inode);
    wr_parent = check_allow_write(parent->inode);

    if (!fsacl_test(acl, svtx)) {
        return wr_self;
    }

    if (current_euid() == dnode->inode->uid) {
        return true;
    }

    return wr_self && wr_parent;
}

int
vfs_do_open(const char* path, int options)
{
    int errno, fd, loptions = 0;
    struct v_dnode *dentry, *file;
    struct v_file* ofile = NULL;
    struct file_locator floc;
    struct v_inode* inode;

    if ((options & FO_CREATE)) {
        loptions |= FLOC_MAYBE_MKNAME;
    } else if ((options & FO_NOFOLLOW)) {
        loptions |= FLOC_NOFOLLOW;
    }

    errno = __vfs_try_locate_file(path, &floc, loptions);

    if (errno || (errno = vfs_alloc_fdslot(&fd))) {
        return errno;
    }

    file   = floc.file;
    dentry = floc.dir;

    if (floc.fresh) {
        errno = __vfs_mknod(dentry->inode, file, VFS_IFFILE, NULL);
        if (errno) {
            vfs_d_free(file);
            __floc_try_unlock(&floc);
            return errno;
        }

        __floc_try_unlock(&floc);
    }


    if ((errno = vfs_open(file, &ofile))) {
        return errno;
    }

    inode = ofile->inode;
    lock_inode(inode);

    struct v_fd* fd_s = cake_grab(fd_pile);
    memset(fd_s, 0, sizeof(*fd_s));

    if ((options & O_TRUNC)) {
        file->inode->fsize = 0;   
    }

    if (vfs_get_dtype(inode->itype) == DT_DIR) {
        ofile->f_pos = 0;
    }
    
    fd_s->file = ofile;
    fd_s->flags = options;
    __current->fdtable->fds[fd] = fd_s;

    unlock_inode(inode);
    
    return fd;
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
    struct lx_dirent* dent = (struct lx_dirent*)dctx->cb_data;
    strncpy(dent->d_name, name, MIN(len, DIRENT_NAME_MAX_LEN));
    dent->d_nlen = len;
    dent->d_type = dtype;
}

__DEFINE_LXSYSCALL2(int, sys_readdir, int, fd, struct lx_dirent*, dent)
{
    struct v_fd* fd_s;
    int errno;

    if ((errno = vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct v_inode* inode = fd_s->file->inode;

    lock_inode(inode);

    if (!check_directory_node(inode)) {
        errno = ENOTDIR;
        goto unlock;
    }

    if (!check_allow_read(inode)) {
        errno = EPERM;
        goto unlock;
    }

    struct dir_context dctx = (struct dir_context) {
        .cb_data = dent,
        .read_complete_callback = __vfs_readdir_callback
    };

    if ((errno = fd_s->file->ops->readdir(fd_s->file, &dctx)) != 1) {
        goto unlock;
    }
    dent->d_offset++;
    fd_s->file->f_pos++;

unlock:
    unlock_inode(inode);

done:
    return DO_STATUS_OR_RETURN(errno);
}

__DEFINE_LXSYSCALL3(int, read, int, fd, void*, buf, size_t, count)
{
    int errno = 0;
    struct v_fd* fd_s;
    struct v_inode* inode;

    if ((errno = vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct v_file* file = fd_s->file;
    if (check_directory_node(file->inode)) {
        errno = EISDIR;
        goto done;
    }

    if (!check_allow_read(file->inode)) {
        errno = EPERM;
        goto done;
    }

    inode = file->inode;
    lock_inode(inode);

    __vfs_touch_inode(inode, INODE_ACCESSED);

    if (check_seqdev_node(inode) || (fd_s->flags & FO_DIRECT)) {
        errno = file->ops->read(inode, buf, count, file->f_pos);
    } else {
        errno = pcache_read(inode, buf, count, file->f_pos);
    }

    if (errno > 0) {
        file->f_pos += errno;
        unlock_inode(inode);
        return errno;
    }

    unlock_inode(inode);

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

    struct v_inode* inode;
    struct v_file* file = fd_s->file;

    if ((errno = vfs_check_writable(file->dnode))) {
        goto done;
    }

    if (check_directory_node(file->inode)) {
        errno = EISDIR;
        goto done;
    }

    inode = file->inode;
    lock_inode(inode);

    __vfs_touch_inode(inode, INODE_MODIFY);
    if ((fd_s->flags & O_APPEND)) {
        file->f_pos = inode->fsize;
    }

    if (check_seqdev_node(inode) || (fd_s->flags & FO_DIRECT)) {
        errno = file->ops->write(inode, buf, count, file->f_pos);
    } else {
        errno = pcache_write(inode, buf, count, file->f_pos);
    }

    if (errno > 0) {
        file->f_pos += errno;
        inode->fsize = MAX(inode->fsize, file->f_pos);

        unlock_inode(inode);
        return errno;
    }

    unlock_inode(inode);

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
    struct v_inode* inode = file->inode;

    if (!file->ops->seek) {
        errno = ENOTSUP;
        goto done;
    }

    if (!check_allow_read(inode)) {
        errno = EPERM;
        goto done;
    }

    lock_inode(inode);

    int overflow = 0;
    int fpos = file->f_pos;

    if (vfs_get_dtype(inode->itype) == DT_DIR) {
        options = (options != FSEEK_END) ? options : FSEEK_SET;
    }
    
    switch (options) {
        case FSEEK_CUR:
            overflow = sadd_of((int)file->f_pos, offset, &fpos);
            break;
        case FSEEK_END:
            overflow = sadd_of((int)inode->fsize, offset, &fpos);
            break;
        case FSEEK_SET:
            fpos = offset;
            break;
    }

    if (overflow) {
        errno = EOVERFLOW;
    }
    else {
        errno = file->ops->seek(file, fpos);
    }

    unlock_inode(inode);

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

    if (!check_symlink_node(inode)) {
        return EINVAL;
    }

    if (!inode->ops->read_symlink) {
        return ENOTSUP;
    }

    if (!check_allow_read(inode)) {
        return EPERM;
    }

    lock_inode(inode);

    int errno = inode->ops->read_symlink(inode, &link);
    if (errno >= 0) {
        strncpy(buf, link, MIN(size, (size_t)errno));
    }

    unlock_inode(inode);
    return errno;
}

int
vfs_get_dtype(int itype)
{
    int dtype = DT_FILE;
    if (check_itype(itype, VFS_IFSYMLINK)) {
        dtype |= DT_SYMLINK;
    }
    
    if (check_itype(itype, VFS_IFDIR)) {
        dtype |= DT_DIR;
        return dtype;
    }

    // TODO other types
    
    return dtype;
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

__DEFINE_LXSYSCALL4(
  int, readlinkat, int, dirfd, const char*, pathname, char*, buf, size_t, size)
{
    int errno;
    struct v_fd* fd_s;
    if ((errno = vfs_getfd(dirfd, &fd_s))) {
        goto done;
    }

    pathname = pathname ? pathname : "";

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

    if (!__check_unlinkable(dnode)) {
        errno = EPERM;
        goto done;
    } 

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

    if (check_directory_node(dnode->inode)) {
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

    if (!(errno = vfs_walk(parent, name_value, &dir, NULL, 0))) {
        errno = EEXIST;
        goto done;
    }

    if ((errno = vfs_check_writable(parent))) {
        goto done;
    }

    if (!(dir = vfs_d_alloc(parent, &name))) {
        errno = ENOMEM;
        goto done;
    }

    struct v_inode* inode = parent->inode;

    lock_dnode(parent);
    lock_inode(inode);

    if ((parent->super_block->fs->types & FSTYPE_ROFS)) {
        errno = ENOTSUP;
    } else if (!inode->ops->mkdir) {
        errno = ENOTSUP;
    } else if (!check_directory_node(inode)) {
        errno = ENOTDIR;
    } else if (!(errno = inode->ops->mkdir(inode, dir))) {
        vfs_dcache_add(parent, dir);
        goto cleanup;
    }

    vfs_d_free(dir);

cleanup:
    unlock_inode(inode);
    unlock_dnode(parent);
done:
    return DO_STATUS(errno);
}

static int
__vfs_do_unlink(struct v_dnode* dnode)
{
    int errno;
    struct v_inode* inode = dnode->inode;

    if (dnode->ref_count > 1) {
        return EBUSY;
    }

    if (!__check_unlinkable(dnode)) {
        return EPERM;
    } 

    if ((errno = vfs_check_writable(dnode))) {
        return errno;
    }

    lock_inode(inode);

    if (inode->open_count) {
        errno = EBUSY;
    } else if (!check_directory_node(inode)) {
        errno = inode->ops->unlink(inode, dnode);
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
    struct file_locator floc;
    struct v_dnode *to_link, *name_file;

    errno = __vfs_try_locate_file(oldpath, &floc, 0);
    if (errno) {
        goto done;
    }

    __floc_try_unlock(&floc);

    to_link = floc.file;
    errno = __vfs_try_locate_file(newpath, &floc, FLOC_MKNAME);
    if (!errno) {
        goto done;       
    }

    name_file = floc.file;
    errno = vfs_link(to_link, name_file);
    if (errno) {
        vfs_d_free(name_file);
    }

done:
    __floc_try_unlock(&floc);
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

    vfs_ref_file(old->file);

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

__DEFINE_LXSYSCALL2(
  int, symlink, const char*, pathname, const char*, link_target)
{
    int errno;
    struct file_locator floc;
    struct v_dnode *file;
    struct v_inode *f_ino;
    
    errno = __vfs_try_locate_file(pathname, &floc, FLOC_MKNAME);
    if (errno) {
        goto done;
    }

    file = floc.file;
    errno = __vfs_mknod(floc.dir->inode, file, VFS_IFSYMLINK, NULL);
    if (errno) {
        vfs_d_free(file);
        goto done;
    }

    f_ino = file->inode;

    assert(f_ino);

    errno = vfs_check_writable(file);
    if (errno) {
        goto done;
    }

    if (!f_ino->ops->set_symlink) {
        errno = ENOTSUP;
        goto done;
    }

    lock_inode(f_ino);

    errno = f_ino->ops->set_symlink(f_ino, link_target);

    unlock_inode(f_ino);

done:
    __floc_try_unlock(&floc);
    return DO_STATUS(errno);
}

static int
vfs_do_chdir_nolock(struct proc_info* proc, struct v_dnode* dnode)
{
    if (!check_directory_node(dnode->inode)) {
        return ENOTDIR;
    }

    if (proc->cwd) {
        vfs_unref_dnode(proc->cwd);
    }

    vfs_ref_dnode(dnode);
    proc->cwd = dnode;

    return 0;
}

static int
vfs_do_chdir(struct proc_info* proc, struct v_dnode* dnode)
{
    int errno = 0;

    lock_dnode(dnode);

    errno = vfs_do_chdir_nolock(proc, dnode);

    unlock_dnode(dnode);

    return errno;
}

__DEFINE_LXSYSCALL1(int, chdir, const char*, path)
{
    struct v_dnode* dnode;
    int errno = 0;

    if ((errno = vfs_walk_proc(path, &dnode, NULL, 0))) {
        goto done;
    }

    errno = vfs_do_chdir((struct proc_info*)__current, dnode);

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

    errno = vfs_do_chdir((struct proc_info*)__current, fd_s->file->dnode);

done:
    return DO_STATUS(errno);
}


__DEFINE_LXSYSCALL1(int, chroot, const char*, path)
{
    int errno;
    struct v_dnode* dnode;
    if ((errno = vfs_walk_proc(path, &dnode, NULL, 0))) {
        return errno;
    }

    lock_dnode(dnode);

    errno = vfs_do_chdir_nolock(__current, dnode);
    if (errno) {
        unlock_dnode(dnode);
        goto done;
    }

    __current->root = dnode;
    
    unlock_dnode(dnode);

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

    buf[len] = '\0';

    ret_ptr = buf;

done:
    syscall_result(errno);
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

    if ((errno = vfs_check_writable(current))) {
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
    vfree((void*)name.value);
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL2(int, fstat, int, fd, struct file_stat*, stat)
{
    int errno = 0;
    struct v_fd* fds;

    if ((errno = vfs_getfd(fd, &fds))) {
        goto done;
    }

    struct v_inode* vino = fds->file->inode;
    struct device* fdev = vino->sb->dev;

    stat->st_ino     = vino->id;
    stat->st_blocks  = vino->lb_usage;
    stat->st_size    = vino->fsize;
    stat->st_blksize = vino->sb->blksize;
    stat->st_nlink   = vino->link_count;
    stat->st_uid     = vino->uid;
    stat->st_gid     = vino->gid;

    stat->st_ctim    = vino->ctime;
    stat->st_atim    = vino->atime;
    stat->st_mtim    = vino->mtime;

    stat->st_mode    = (vino->itype << 16) | vino->acl;

    stat->st_ioblksize = PAGE_SIZE;

    if (check_device_node(vino)) {
        struct device* rdev = resolve_device(vino->data);
        if (!rdev) {
            errno = EINVAL;
            goto done;
        }

        stat->st_rdev = (dev_t){.meta = rdev->ident.fn_grp,
                                .unique = rdev->ident.unique,
                                .index = dev_uid(rdev) };
    }

    if (fdev) {
        stat->st_dev = (dev_t){.meta = fdev->ident.fn_grp,
                               .unique = fdev->ident.unique,
                               .index = dev_uid(fdev) };
    }

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL4(int, fchmodat, int, fd, 
                    const char*, path, int, mode, int, flags)
{
    int errno;
    struct v_dnode *dnode;
    struct v_inode* inode;

    errno = vfs_walkat(fd, path, flags, &dnode);
    if (errno) {
        goto done;
    }

    errno = vfs_check_writable(dnode);
    if (errno) {
        return errno;
    }

    inode = dnode->inode;
    lock_inode(inode);

    if (!current_is_root()) {
        mode = mode & FSACL_RWXMASK;
    }

    inode->acl = mode;
    __vfs_touch_inode(inode, INODE_MODIFY);

    unlock_inode(inode);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL5(int, fchownat, int, fd, 
                    const char*, path, uid_t, uid, gid_t, gid, int, flags)
{
    int errno;
    struct v_dnode *dnode;
    struct v_inode *inode;

    errno = vfs_walkat(fd, path, flags, &dnode);
    if (errno) {
        goto done;
    }

    errno = vfs_check_writable(dnode);
    if (errno) {
        return errno;
    }

    inode = dnode->inode;
    lock_inode(inode);

    inode->uid = uid;
    inode->gid = gid;
    __vfs_touch_inode(inode, INODE_MODIFY);

    unlock_inode(inode);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL4(int, faccessat, int, fd, 
                    const char*, path, int, amode, int, flags)
{
    int errno, acl;
    struct v_dnode *dnode;
    struct v_inode *inode;
    struct user_scope* uscope;

    uid_t tuid;
    gid_t tgid;

    errno = vfs_walkat(fd, path, flags, &dnode);
    if (errno) {
        goto done;
    }

    if ((flags & AT_EACCESS)) {
        tuid = current_euid();
        tgid = current_egid();
    }
    else {
        uscope = current_user_scope();
        tuid = uscope->ruid;
        tgid = uscope->rgid;
    }

    inode = dnode->inode;

    acl  = inode->acl;
    acl &= amode;
    acl &= check_acl_between(inode->uid, inode->gid, tuid, tgid);
    if (!acl) {
        errno = EACCESS;
    }

done:
    return DO_STATUS(errno);
}