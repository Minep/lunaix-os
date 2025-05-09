#include <lunaix/foptions.h>
#include <lunaix/fs/api.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/syscall_utils.h>
#include <lunaix/syslog.h>
#include <lunaix/types.h>

LOG_MODULE("fs")

struct llist_header all_mnts = { .next = &all_mnts, .prev = &all_mnts };

static void
__vfs_attach_vmnt(struct v_dnode* mnt_point, struct v_mount* vmnt)
{
    vmnt->mnt_point = mnt_point;
    vfs_d_assign_vmnt(mnt_point, vmnt);
    vfs_ref_dnode(mnt_point);
}

static struct v_mount*
__vfs_create_mount(struct v_mount* parent, struct v_superblock* mnt_sb)
{
    struct v_mount* mnt = vzalloc(sizeof(struct v_mount));
    if (!mnt) {
        return NULL;
    }

    llist_init_head(&mnt->submnts);
    llist_init_head(&mnt->sibmnts);
    llist_append(&all_mnts, &mnt->list);
    mutex_init(&mnt->lock);

    mnt->parent = parent;
    vfs_vmnt_assign_sb(mnt, mnt_sb);

    if (parent) {
        mnt_mkbusy(parent);
        mutex_lock(&mnt->parent->lock);
        llist_append(&parent->submnts, &mnt->sibmnts);
        mutex_unlock(&mnt->parent->lock);
    }

    return mnt;
}

static void
__vfs_detach_vmnt(struct v_mount* mnt)
{
    assert(llist_empty(&mnt->submnts));

    vfs_unref_dnode(mnt->mnt_point);
    assert(!mnt->busy_counter);

    if (mnt->parent) {
        mnt_chillax(mnt->parent);
    }

    mnt->mnt_point->mnt = NULL;

    llist_delete(&mnt->sibmnts);
    llist_delete(&mnt->list);
    vfree(mnt);
}

static inline void
__detach_node_cache_ref(struct hbucket* bucket)
{
    if (!bucket->head) {
        return;
    }

    bucket->head->pprev = 0;
}

static int
__vfs_do_unmount(struct v_mount* mnt)
{
    int errno = 0;
    struct v_superblock* sb = mnt->super_block;

    if ((errno = sb->fs->unmount(sb))) {
        return errno;
    }

    // detached the inodes from cache, and let lru policy to recycle them
    for (size_t i = 0; i < VFS_HASHTABLE_SIZE; i++) {
        __detach_node_cache_ref(&sb->i_cache.pool[i]);
        __detach_node_cache_ref(&sb->d_cache.pool[i]);
    }

    struct v_dnode *pos, *next;
    llist_for_each(pos, next, &mnt->mnt_point->children, siblings)
    {
        vfs_d_free(pos);
    }

    __vfs_detach_vmnt(mnt);
    vfs_d_assign_vmnt(mnt->mnt_point, mnt->parent);

    return errno;
}

void
mnt_mkbusy(struct v_mount* mnt)
{
    mutex_lock(&mnt->lock);
    mnt->busy_counter++;
    mutex_unlock(&mnt->lock);
}

void
mnt_chillax(struct v_mount* mnt)
{
    mutex_lock(&mnt->lock);
    mnt->busy_counter--;
    mutex_unlock(&mnt->lock);
}

int
vfs_mount_root(const char* fs_name, struct device* device)
{
    extern struct v_dnode* vfs_sysroot;
    int errno = 0;
    if (vfs_sysroot->mnt && (errno = vfs_unmount_at(vfs_sysroot))) {
        return errno;
    }
    return vfs_mount_at(fs_name, device, vfs_sysroot, 0);
}

int
vfs_mount(const char* target,
          const char* fs_name,
          struct device* device,
          int options)
{
    int errno;
    struct v_dnode* mnt;

    if (!(errno =
            vfs_walk(__current->cwd, target, &mnt, NULL, VFS_WALK_MKPARENT))) {
        errno = vfs_mount_at(fs_name, device, mnt, options);
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

static int
vfs_mount_fsat(struct filesystem* fs,
               struct device* device,
               struct v_dnode* mnt_point,
               int options)
{

    if (device && device->dev_type != DEV_IFVOL) {
        return ENOTBLK;
    }

    if (mnt_point->inode && !check_directory_node(mnt_point->inode)) {
        return ENOTDIR;
    }

    if ((fs->types & FSTYPE_ROFS)) {
        options |= MNT_RO;
    }

    if (!(fs->types & FSTYPE_PSEUDO) && !device) {
        return ENODEV;
    }

    int errno = 0;
    char* dev_name;
    char* fsname;
    struct v_mount *parent_mnt, *vmnt;
    struct v_superblock *sb;

    fsname = HSTR_VAL(fs->fs_name);
    parent_mnt = mnt_point->mnt;
    sb = vfs_sb_alloc();

    dev_name = device ? device->name_val : "sys";

    // prepare v_superblock for fs::mount invoke
    sb->dev = device;
    sb->fs = fs;
    sb->root = mnt_point;
    
    vmnt = __vfs_create_mount(parent_mnt, sb);

    if (!vmnt) {
        errno = ENOMEM;
        goto cleanup;
    }

    __vfs_attach_vmnt(mnt_point, vmnt);

    mnt_point->mnt->flags = options;
    if (!(errno = fs->mount(sb, mnt_point))) {
        kprintf("mount: dev=%s, fs=%s, mode=%d", 
                    dev_name, fsname, options);
    } else {
        goto cleanup;
    }

    return errno;

cleanup:
    ERROR("failed mount: dev=%s, fs=%s, mode=%d, err=%d",
            dev_name, fsname, options, errno);

    __vfs_detach_vmnt(mnt_point->mnt);
    vfs_d_assign_vmnt(mnt_point, parent_mnt);

    mnt_point->mnt = parent_mnt;

    return errno;
}

int
vfs_mount_at(const char* fs_name,
             struct device* device,
             struct v_dnode* mnt_point,
             int options)
{
    if (fs_name) {
        struct filesystem* fs = fsm_get(fs_name);
        if (!fs) {
            return ENODEV;
        }

        return vfs_mount_fsat(fs, device, mnt_point, options);
    }

    int errno = ENODEV;
    struct fs_iter fsi;

    fsm_itbegin(&fsi);
    while (fsm_itnext(&fsi))
    {
        if ((fsi.fs->types & FSTYPE_PSEUDO)) {
            continue;
        }

        INFO("mount attempt: %s", HSTR_VAL(fsi.fs->fs_name));
        errno = vfs_mount_fsat(fsi.fs, device, mnt_point, options);
        if (!errno) {
            break;
        }
    }

    return errno;
}

int
vfs_unmount_at(struct v_dnode* mnt_point)
{
    int errno = 0;
    struct v_superblock* sb;

    sb = mnt_point->super_block;
    if (!sb) {
        return EINVAL;
    }

    if (sb->root != mnt_point) {
        return EINVAL;
    }

    if (mnt_check_busy(mnt_point->mnt)) {
        return EBUSY;
    }

    return __vfs_do_unmount(mnt_point->mnt);
}

int
vfs_check_writable(struct v_dnode* dnode)
{
    if ((dnode->mnt->flags & MNT_RO)) {
        return EROFS;
    }

    if (!check_allow_write(dnode->inode)) {
        return EPERM;
    }

    return 0;
}

__DEFINE_LXSYSCALL4(int,
                    mount,
                    const char*,
                    source,
                    const char*,
                    target,
                    const char*,
                    fstype,
                    int,
                    options)
{
    struct device* device = NULL;
    struct v_dnode *dev = NULL, *mnt = NULL;
    int errno = 0;

    // It is fine if source is not exist, as some mounting don't require it
    vfs_walk(__current->cwd, source, &dev, NULL, 0);

    if ((errno = vfs_walk(__current->cwd, target, &mnt, NULL, 0))) {
        goto done;
    }

    if (mnt->ref_count > 1) {
        errno = EBUSY;
        goto done;
    }

    if (mnt->mnt->mnt_point == mnt) {
        errno = EBUSY;
        goto done;
    }

    if (dev) {
        if (!check_voldev_node(dev->inode)) {
            errno = ENOTDEV;
            goto done;
        }

        device = resolve_device(dev->inode->data);
        assert(device);
    }

    errno = vfs_mount_at(fstype, device, mnt, options);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL1(int, unmount, const char*, target)
{
    return vfs_unmount(target);
}