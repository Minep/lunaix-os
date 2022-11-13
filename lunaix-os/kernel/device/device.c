#include <klibc/stdio.h>
#include <lunaix/device.h>
#include <lunaix/fs.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/ioctl.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>

static DEFINE_LLIST(root_list);

static volatile dev_t devid = 0;

struct device*
device_add(struct device* parent,
           void* underlay,
           char* name_fmt,
           u32_t type,
           va_list args)
{
    struct device* dev = vzalloc(sizeof(struct device));

    if (parent) {
        assert((parent->dev_type & DEV_MSKIF) == DEV_IFCAT);
        llist_append(&parent->children, &dev->siblings);
    } else {
        llist_append(&root_list, &dev->siblings);
    }

    size_t strlen =
      __ksprintf_internal(dev->name_val, name_fmt, DEVICE_NAME_SIZE, args);

    dev->magic = DEV_STRUCT_MAGIC;
    dev->dev_id = devid++;
    dev->name = HSTR(dev->name_val, strlen);
    dev->parent = parent;
    dev->underlay = underlay;
    dev->dev_type = type;

    hstr_rehash(&dev->name, HSTR_FULL_HASH);
    llist_init_head(&dev->children);

    return dev;
}

struct device*
device_addseq(struct device* parent, void* underlay, char* name_fmt, ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device* dev =
      device_add(parent, underlay, name_fmt, DEV_IFSEQ, args);

    va_end(args);
    return dev;
}

struct device*
device_addvol(struct device* parent, void* underlay, char* name_fmt, ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device* dev =
      device_add(parent, underlay, name_fmt, DEV_IFVOL, args);

    va_end(args);
    return dev;
}

struct device*
device_addcat(struct device* parent, char* name_fmt, ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device* dev = device_add(parent, NULL, name_fmt, DEV_IFCAT, args);

    va_end(args);
    return dev;
}

struct device*
device_getbyid(struct llist_header* devlist, dev_t id)
{
    devlist = devlist ? devlist : &root_list;
    struct device *pos, *n;
    llist_for_each(pos, n, devlist, siblings)
    {
        if (pos->dev_id == id) {
            return pos;
        }
    }

    return NULL;
}

struct device*
device_getbyhname(struct device* root_dev, struct hstr* name)
{
    struct llist_header* devlist = root_dev ? &root_dev->children : &root_list;
    struct device *pos, *n;
    llist_for_each(pos, n, devlist, siblings)
    {
        if (HSTR_EQ(&pos->name, name)) {
            return pos;
        }
    }

    return NULL;
}

struct device*
device_getbyname(struct device* root_dev, const char* name, size_t len)
{
    struct hstr hname = HSTR(name, len);
    hstr_rehash(&hname, HSTR_FULL_HASH);

    return device_getbyhname(root_dev, &hname);
}

void
device_remove(struct device* dev)
{
    llist_delete(&dev->siblings);
    vfree(dev);
}

struct device*
device_getbyoffset(struct device* root_dev, int offset)
{
    struct llist_header* devlist = root_dev ? &root_dev->children : &root_list;
    struct device *pos, *n;
    int off = 0;
    llist_for_each(pos, n, devlist, siblings)
    {
        if (off++ >= offset) {
            return pos;
        }
    }
    return NULL;
}

__DEFINE_LXSYSCALL3(int, ioctl, int, fd, int, req, va_list, args)
{
    int errno;
    struct v_fd* fd_s;
    if ((errno = vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct device* dev = (struct device*)fd_s->file->inode->data;
    if (dev->magic != DEV_STRUCT_MAGIC) {
        errno = ENODEV;
        goto done;
    }

    if (!dev->exec_cmd) {
        errno = EINVAL;
        goto done;
    }

    errno = dev->exec_cmd(dev, req, args);

done:
    return DO_STATUS_OR_RETURN(errno);
}