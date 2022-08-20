#include <klibc/string.h>
#include <lunaix/fs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/syscall.h>

struct v_xattr_entry*
xattr_new(struct hstr* name)
{
    struct v_xattr_entry* entry = valloc(sizeof(*entry));
    if (!entry) {
        return NULL;
    }
    *entry =
      (struct v_xattr_entry){ .name = HHSTR(valloc(VFS_NAME_MAXLEN), 0, 0) };

    hstrcpy(&entry->name, name);
    return entry;
}

void
xattr_free(struct v_xattr_entry* entry)
{
    vfree(entry->name.value);
    vfree(entry);
}

struct v_xattr_entry*
xattr_getcache(struct v_inode* inode, struct hstr* name)
{
    struct v_xattr_entry *pos, *n;
    llist_for_each(pos, n, &inode->xattrs, entries)
    {
        if (HSTR_EQ(&pos->name, name)) {
            return pos;
        }
    }

    return NULL;
}

void
xattr_addcache(struct v_inode* inode, struct v_xattr_entry* xattr)
{
    llist_append(&inode->xattrs, &xattr->entries);
}

void
xattr_delcache(struct v_inode* inode, struct v_xattr_entry* xattr)
{
    llist_delete(&xattr->entries);
}

int
__vfs_getxattr(struct v_inode* inode,
               struct v_xattr_entry** xentry,
               const char* name)
{
    if (!inode->ops->getxattr) {
        return ENOTSUP;
    }

    int errno = 0;
    size_t len = strlen(name);

    if (len > VFS_NAME_MAXLEN) {
        return ERANGE;
    }

    struct hstr hname = HSTR(name, len);

    hstr_rehash(&hname, HSTR_FULL_HASH);

    struct v_xattr_entry* entry = xattr_getcache(inode, &hname);
    if (!entry) {
        if (!(entry = xattr_new(&hname))) {
            return ENOMEM;
        }
    }

    if (!(errno = inode->ops->getxattr(inode, entry))) {
        *xentry = entry;
        xattr_addcache(inode, entry);
    } else {
        xattr_free(entry);
    }
    return errno;
}

int
__vfs_setxattr(struct v_inode* inode,
               const char* name,
               const void* data,
               size_t len)
{
    if (!inode->ops->setxattr || !inode->ops->delxattr) {
        return ENOTSUP;
    }

    int errno = 0;
    size_t slen = strlen(name);

    if (slen > VFS_NAME_MAXLEN) {
        return ERANGE;
    }

    struct hstr hname = HSTR(name, slen);

    hstr_rehash(&hname, HSTR_FULL_HASH);

    struct v_xattr_entry* entry = xattr_getcache(inode, &hname);
    if (!entry) {
        if (!(entry = xattr_new(&hname))) {
            return ENOMEM;
        }
    } else {
        xattr_delcache(inode, entry);
    }

    if ((errno = inode->ops->delxattr(inode, entry))) {
        xattr_free(entry);
        goto done;
    }

    entry->value = data;
    entry->len = len;

    if ((errno = inode->ops->setxattr(inode, entry))) {
        xattr_free(entry);
        goto done;
    }

    xattr_addcache(inode, entry);
done:
    return errno;
}

__DEFINE_LXSYSCALL4(int,
                    getxattr,
                    const char*,
                    path,
                    const char*,
                    name,
                    void*,
                    value,
                    size_t,
                    len)
{
    struct v_dnode* dnode;
    struct v_xattr_entry* xattr;
    int errno = 0;

    if ((errno = vfs_walk_proc(path, &dnode, NULL, 0))) {
        goto done;
    }

    if ((errno = __vfs_getxattr(dnode->inode, &xattr, name))) {
        goto done;
    }

    if (len < xattr->len) {
        errno = ERANGE;
        goto done;
    }

    memcpy(value, xattr->value, len);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL4(int,
                    setxattr,
                    const char*,
                    path,
                    const char*,
                    name,
                    void*,
                    value,
                    size_t,
                    len)
{
    struct v_dnode* dnode;
    struct v_xattr_entry* xattr;
    int errno = 0;

    if ((errno = vfs_walk_proc(path, &dnode, NULL, 0))) {
        goto done;
    }

    if ((errno = __vfs_setxattr(dnode->inode, name, value, len))) {
        goto done;
    }

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL4(int,
                    fgetxattr,
                    int,
                    fd,
                    const char*,
                    name,
                    void*,
                    value,
                    size_t,
                    len)
{
    struct v_fd* fd_s;
    struct v_xattr_entry* xattr;
    int errno = 0;

    if ((errno = vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    if ((errno = __vfs_getxattr(fd_s->file->inode, &xattr, name))) {
        goto done;
    }

    if (len < xattr->len) {
        errno = ERANGE;
        goto done;
    }

    memcpy(value, xattr->value, len);

done:
    return DO_STATUS(errno);
}

__DEFINE_LXSYSCALL4(int,
                    fsetxattr,
                    int,
                    fd,
                    const char*,
                    name,
                    void*,
                    value,
                    size_t,
                    len)
{
    struct v_fd* fd_s;
    struct v_xattr_entry* xattr;
    int errno = 0;

    if ((errno = vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    if ((errno = __vfs_setxattr(fd_s->file->inode, name, value, len))) {
        goto done;
    }

done:
    return DO_STATUS(errno);
}