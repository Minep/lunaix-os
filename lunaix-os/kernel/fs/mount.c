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

struct v_mount*
vfs_create_mount(struct v_mount* parent, struct v_dnode* mnt_point)
{
    struct v_mount* mnt = vzalloc(sizeof(struct v_mount));
    if (!mnt) {
        return NULL;
    }

    llist_init_head(&mnt->submnts);
    llist_append(&all_mnts, &mnt->list);
    mutex_init(&mnt->lock);

    mnt->parent = parent;
    mnt->mnt_point = mnt_point;
    vfs_vmnt_assign_sb(mnt, mnt_point->super_block);

    if (parent) {
        mnt_mkbusy(parent);
        mutex_lock(&mnt->parent->lock);
        llist_append(&parent->submnts, &mnt->sibmnts);
        mutex_unlock(&mnt->parent->lock);
    }
    
    atomic_fetch_add(&mnt_point->ref_count, 1);

    return mnt;
}

void
__vfs_release_vmnt(struct v_mount* mnt)
{
    assert(llist_empty(&mnt->submnts));

    if (mnt->parent) {
        mnt_chillax(mnt->parent);
    }

    llist_delete(&mnt->sibmnts);
    llist_delete(&mnt->list);
    atomic_fetch_sub(&mnt->mnt_point->ref_count, 1);
    vfree(mnt);
}

int
__vfs_do_unmount(struct v_mount* mnt)
{
    int errno = 0;
    struct v_superblock* sb = mnt->super_block;

    if ((errno = sb->fs->unmount(sb))) {
        return errno;
    }

    // detached the inodes from cache, and let lru policy to recycle them
    for (size_t i = 0; i < VFS_HASHTABLE_SIZE; i++) {
        struct hbucket* bucket = &sb->i_cache[i];
        if (!bucket) {
            continue;
        }
        bucket->head->pprev = 0;
    }

    mnt->mnt_point->mnt = mnt->parent;

    vfs_sb_free(sb);
    __vfs_release_vmnt(mnt);

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

int
vfs_mount_at(const char* fs_name,
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

    struct filesystem* fs = fsm_get(fs_name);
    if (!fs) {
        return ENODEV;
    }

    if ((fs->types & FSTYPE_ROFS)) {
        options |= MNT_RO;
    }

    if (!(fs->types & FSTYPE_PSEUDO) && !device) {
        return ENODEV;
    }

    int errno = 0;
    char* dev_name = "sys";
    struct v_mount* parent_mnt = mnt_point->mnt;
    struct v_superblock *sb = vfs_sb_alloc(), 
                        *old_sb = mnt_point->super_block;

    if (device) {
        dev_name = device->name_val;
    }

    // prepare v_superblock for fs::mount invoke
    sb->dev = device;
    sb->fs = fs;
    sb->root = mnt_point;
    vfs_d_assign_sb(mnt_point, sb);

    if (!(mnt_point->mnt = vfs_create_mount(parent_mnt, mnt_point))) {
        errno = ENOMEM;
        goto cleanup;
    }

    mnt_point->mnt->flags = options;
    if (!(errno = fs->mount(sb, mnt_point))) {
        kprintf("mount: dev=%s, fs=%s, mode=%d", dev_name, fs_name, options);
    } else {
        goto cleanup;
    }

    vfs_sb_free(old_sb);
    return errno;

cleanup:
    ERROR("failed mount: dev=%s, fs=%s, mode=%d, err=%d",
          dev_name,
          fs_name,
          options,
          errno);
    vfs_d_assign_sb(mnt_point, old_sb);
    vfs_sb_free(sb);
    __vfs_release_vmnt(mnt_point->mnt);

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

    if (sb->root != mnt_point) {
        return EINVAL;
    }

    if (mnt_point->mnt->busy_counter) {
        return EBUSY;
    }

    if (!(errno = __vfs_do_unmount(mnt_point->mnt))) {
        atomic_fetch_sub(&mnt_point->ref_count, 1);
    }

    return errno;
}

int
vfs_check_writable(struct v_dnode* dnode)
{
    if ((dnode->mnt->flags & MNT_RO)) {
        return EROFS;
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

    // By our convention.
    // XXX could we do better?
    struct device* device = NULL;

    if (dev) {
        if (!check_voldev_node(dev->inode)) {
            errno = ENOTDEV;
            goto done;
        }
        device = (struct device*)dev->inode->data;
    }

    errno = vfs_mount_at(fstype, device, mnt, options);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL1(int, unmount, const char*, target)
{
    return vfs_unmount(target);
}