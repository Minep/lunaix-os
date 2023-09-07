
#include <lunaix/device.h>
#include <lunaix/fs.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/ioctl.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

#include <usr/lunaix/device.h>

#include <klibc/stdio.h>
#include <klibc/string.h>

static DEFINE_LLIST(root_list);

static volatile u32_t devid = 0;

struct devclass default_devclass = {};

void
device_prepare(struct device* dev, struct devclass* class)
{
    dev->magic = DEV_STRUCT_MAGIC;
    dev->dev_uid = devid++;
    dev->class = class ? class : &default_devclass;

    llist_init_head(&dev->children);
}

static void
device_setname_vargs(struct device* dev, char* fmt, va_list args)
{
    size_t strlen =
      __ksprintf_internal(dev->name_val, fmt, DEVICE_NAME_SIZE, args);

    dev->name = HSTR(dev->name_val, strlen);

    hstr_rehash(&dev->name, HSTR_FULL_HASH);
}

void
device_setname(struct device* dev, char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    device_setname_vargs(dev, fmt, args);

    va_end(args);
}

struct device*
device_add_vargs(struct device* parent,
                 void* underlay,
                 char* name_fmt,
                 u32_t type,
                 struct devclass* class,
                 va_list args)
{
    struct device* dev = vzalloc(sizeof(struct device));

    device_prepare(dev, class);

    if (parent) {
        assert((parent->dev_type & DEV_MSKIF) == DEV_IFCAT);
        llist_append(&parent->children, &dev->siblings);
    } else {
        llist_append(&root_list, &dev->siblings);
    }

    if (name_fmt) {
        device_setname_vargs(dev, name_fmt, args);
    }

    dev->parent = parent;
    dev->underlay = underlay;
    dev->dev_type = type;

    return dev;
}

struct device*
device_add(struct device* parent,
           struct devclass* class,
           void* underlay,
           u32_t type,
           char* name_fmt,
           ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device* dev =
      device_add_vargs(parent, underlay, name_fmt, type, class, args);

    va_end(args);
    return dev;
}

struct device*
device_addsys(struct device* parent,
              struct devclass* class,
              void* underlay,
              char* name_fmt,
              ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device* dev =
      device_add_vargs(parent, underlay, name_fmt, DEV_IFSYS, class, args);

    va_end(args);
    return dev;
}

struct device*
device_addseq(struct device* parent,
              struct devclass* class,
              void* underlay,
              char* name_fmt,
              ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device* dev =
      device_add_vargs(parent, underlay, name_fmt, DEV_IFSEQ, class, args);

    va_end(args);
    return dev;
}

struct device*
device_addvol(struct device* parent,
              struct devclass* class,
              void* underlay,
              char* name_fmt,
              ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device* dev =
      device_add_vargs(parent, underlay, name_fmt, DEV_IFVOL, class, args);

    va_end(args);
    return dev;
}

struct device*
device_addcat(struct device* parent, char* name_fmt, ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device* dev =
      device_add_vargs(parent, NULL, name_fmt, DEV_IFCAT, NULL, args);

    va_end(args);
    return dev;
}

struct device*
device_getbyid(struct llist_header* devlist, u32_t id)
{
    devlist = devlist ? devlist : &root_list;
    struct device *pos, *n;
    llist_for_each(pos, n, devlist, siblings)
    {
        if (pos->dev_uid == id) {
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

static inline void
device_populate_info(struct device* dev, struct dev_info* devinfo)
{
    devinfo->dev_id.meta = dev->class->meta;
    devinfo->dev_id.device = dev->class->device;
    devinfo->dev_id.variant = dev->class->variant;

    if (!devinfo->dev_name.buf) {
        return;
    }

    struct device_def* def = devdef_byclass(dev->class);
    size_t buflen = devinfo->dev_name.buf_len;

    strncpy(devinfo->dev_name.buf, def->name, buflen);
    devinfo->dev_name.buf[buflen - 1] = 0;
}

__DEFINE_LXSYSCALL3(int, ioctl, int, fd, int, req, va_list, args)
{
    int errno = -1;
    struct v_fd* fd_s;
    if ((errno &= vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct device* dev = (struct device*)fd_s->file->inode->data;
    if (dev->magic != DEV_STRUCT_MAGIC) {
        errno &= ENODEV;
        goto done;
    }

    if (req == DEVIOIDENT) {
        struct dev_info* devinfo = va_arg(args, struct dev_info*);
        device_populate_info(dev, devinfo);
        errno = 0;
    }

    if (!dev->ops.exec_cmd) {
        errno &= ENOTSUP;
        goto done;
    }

    errno &= dev->ops.exec_cmd(dev, req, args);

done:
    return DO_STATUS_OR_RETURN(errno);
}