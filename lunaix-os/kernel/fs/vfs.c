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

struct hstr vfs_ddot = HSTR("..", 2);
struct hstr vfs_dot = HSTR(".", 1);
struct hstr vfs_empty = HSTR("", 0);

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

    hstr_rehash(&vfs_ddot, HSTR_FULL_HASH);
    hstr_rehash(&vfs_dot, HSTR_FULL_HASH);

    // 创建一个根superblock，用来蕴含我们的根目录。
    root_sb = vfs_sb_alloc();
    root_sb->root = vfs_d_alloc();
    root_sb->root->inode = vfs_i_alloc();
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
    if (!str->len || HSTR_EQ(str, &vfs_dot))
        return parent;

    if (HSTR_EQ(str, &vfs_ddot)) {
        return parent->parent ? parent->parent : parent;
    }

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
            dnode->name = HHSTR(valloc(VFS_NAME_MAXLEN), j, name.hash);

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
    char* pathname = path;
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
vfs_mount(const char* target, const char* fs_name, bdev_t device)
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
vfs_mount_at(const char* fs_name, bdev_t device, struct v_dnode* mnt_point)
{
    struct filesystem* fs = fsm_get(fs_name);
    if (!fs)
        return ENODEV;
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
        return EINVAL;
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

    struct v_inode* inode = dnode->inode;
    struct v_file* vfile = cake_grab(file_pile);
    memset(vfile, 0, sizeof(*vfile));

    vfile->dnode = dnode;
    vfile->inode = inode;
    vfile->ref_count = 1;
    vfile->ops = inode->default_fops;

    if ((inode->itype & VFS_IFFILE) && !inode->pg_cache) {
        struct pcache* pcache = vzalloc(sizeof(struct pcache));
        pcache_init(pcache);
        inode->pg_cache = pcache;
    }

    int errno = inode->ops.open(inode, vfile);
    if (errno) {
        cake_release(file_pile, vfile);
    } else {
        dnode->ref_count++;
        inode->open_count++;
        *file = vfile;
    }

    return errno;
}

int
vfs_link(struct v_dnode* to_link, struct v_dnode* name)
{
    int errno;
    if (to_link->super_block->root != name->super_block->root) {
        errno = EXDEV;
    } else if (!to_link->inode->ops.link) {
        errno = ENOTSUP;
    } else if (!(errno = to_link->inode->ops.link(to_link->inode, name))) {
        name->inode = to_link->inode;
        to_link->inode->link_count++;
    }

    return errno;
}

int
vfs_close(struct v_file* file)
{
    int errno = 0;
    if (!file->ops.close || !(errno = file->ops.close(file))) {
        file->dnode->ref_count--;
        file->inode->open_count--;
        pcache_commit_all(file);
        cake_release(file_pile, file);
    }
    return errno;
}

int
vfs_fsync(struct v_file* file)
{
    int errno = ENOTSUP;
    pcache_commit_all(file);
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
    memset(dnode, 0, sizeof(*dnode));
    llist_init_head(&dnode->children);
    dnode->name = vfs_empty;
    return dnode;
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

    if (!(errno = (*fdir)->inode->ops.create((*fdir)->inode))) {
        struct v_dnode* file_new;
        file_new = vfs_d_alloc();
        file_new->name = HHSTR(valloc(VFS_NAME_MAXLEN), name.len, name.hash);
        strcpy(file_new->name.value, name_str);
        *file = file_new;

        llist_append(&(*fdir)->children, &file_new->siblings);
    }

    return errno;
}

int
vfs_do_open(const char* path, int options)
{
    int errno, fd;
    struct v_dnode *dentry, *file;
    struct v_file* ofile = 0;

    errno = __vfs_try_locate_file(path, &dentry, &file, 0);

    if (errno != ENOENT && (options & FO_CREATE)) {
        errno = dentry->inode->ops.create(dentry->inode);
    }

    if (!errno && (errno = vfs_open(file, &ofile))) {
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
            if ((errno = fd_s->file->ops.readdir(fd_s->file, &dctx))) {
                goto done;
            }
        }
        errno = 0;
        dent->d_offset++;
    }

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL1(int, mkdir, const char*, path)
{
    struct v_dnode *parent, *dir;
    struct hstr component = HSTR(valloc(VFS_NAME_MAXLEN), 0);
    int errno =
      vfs_walk(__current->cwd, path, &parent, &component, VFS_WALK_PARENT);
    if (errno) {
        goto done;
    }

    if ((parent->super_block->fs->types & FSTYPE_ROFS)) {
        errno = ENOTSUP;
    } else if (!parent->inode->ops.mkdir) {
        errno = ENOTSUP;
    } else if (!(parent->inode->itype & VFS_IFDIR)) {
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

    cpu_enable_interrupt();
    errno = file->ops.read(file, buf, count, file->f_pos);
    cpu_disable_interrupt();

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

    cpu_enable_interrupt();
    errno = file->ops.write(file, buf, count, file->f_pos);
    cpu_disable_interrupt();

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
    if (!file->ops.seek || !(errno = file->ops.seek(file, fpos))) {
        file->f_pos = fpos;
    }

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
    char* link;
    if (dnode->inode->ops.read_symlink) {
        int errno = dnode->inode->ops.read_symlink(dnode->inode, &link);
        strncpy(buf, link, size);
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

__DEFINE_LXSYSCALL1(int, rmdir, const char*, pathname)
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

    if (dnode->inode->open_count) {
        errno = EBUSY;
        goto done;
    }

    if ((dnode->inode->itype & VFS_IFDIR)) {
        errno = dnode->inode->ops.rmdir(dnode->inode);
    } else {
        errno = ENOTDIR;
    }

done:
    return DO_STATUS(errno);
}

int
__vfs_do_unlink(struct v_inode* inode)
{
    int errno;
    if (inode->open_count) {
        errno = EBUSY;
    } else if (!(inode->itype & VFS_IFDIR)) {
        // TODO handle symbolic link and type other than regular file
        errno = inode->ops.unlink(inode);
        if (!errno) {
            inode->link_count--;
        }
    } else {
        errno = EISDIR;
    }

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

    errno = __vfs_do_unlink(dnode->inode);

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
        errno = __vfs_do_unlink(dnode->inode);
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
    old->file->ref_count++;

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
    if (newfd_s && (errno = vfs_close(newfd_s))) {
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

    errno = dnode->inode->ops.symlink(dnode->inode, link_target);

done:
    return DO_STATUS(errno);
}

int
__vfs_do_chdir(struct v_dnode* dnode)
{
    int errno = 0;
    if (!(dnode->inode->itype & VFS_IFDIR)) {
        errno = ENOTDIR;
        goto done;
    }

    if (__current->cwd) {
        __current->cwd->ref_count--;
    }

    dnode->ref_count++;
    __current->cwd = dnode;

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

    if (!__current->cwd) {
        *buf = PATH_DELIM;
        goto done;
    }

    size_t len = vfs_get_path(__current->cwd, buf, size, 0);
    if (len == size) {
        errno = ERANGE;
        goto done;
    }

    buf[len + 1] = '\0';

    ret_ptr = buf;

done:
    __current->k_status = errno;
    return ret_ptr;
}