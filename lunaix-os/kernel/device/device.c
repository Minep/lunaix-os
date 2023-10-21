
#include <lunaix/device.h>
#include <lunaix/fs.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/ioctl.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

#include <klibc/stdio.h>
#include <klibc/string.h>

static DEFINE_LLIST(root_list);

static volatile u32_t devid = 0;

struct devclass default_devclass = {};

void
device_setname_vargs(struct device* dev, char* fmt, va_list args)
{
    size_t strlen =
      __ksprintf_internal(dev->name_val, fmt, DEVICE_NAME_SIZE, args);

    dev->name = HSTR(dev->name_val, strlen);

    hstr_rehash(&dev->name, HSTR_FULL_HASH);
}

void
device_register(struct device* dev, struct devclass* class, char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (fmt) {
        device_setname_vargs(dev, fmt, args);
    }

    if (class) {
        dev->ident = (struct devident){ .fn_grp = class->fn_grp,
                                        .unique = DEV_UNIQUE(class->device,
                                                             class->variant) };
    }

    dev->dev_uid = devid++;

    struct device* parent = dev->parent;
    if (parent) {
        assert((parent->dev_type & DEV_MSKIF) == DEV_IFCAT);
        llist_append(&parent->children, &dev->siblings);
    } else {
        llist_append(&root_list, &dev->siblings);
    }

    va_end(args);
}

void
device_create(struct device* dev,
              struct device* parent,
              u32_t type,
              void* underlay)
{
    dev->magic = DEV_STRUCT_MAGIC;
    dev->underlay = underlay;
    dev->dev_type = type;
    dev->parent = parent;

    llist_init_head(&dev->children);
    mutex_init(&dev->lock);
}

struct device*
device_alloc(struct device* parent, u32_t type, void* underlay)
{
    struct device* dev = vzalloc(sizeof(struct device));

    if (!dev) {
        return NULL;
    }

    device_create(dev, parent, type, underlay);

    return dev;
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
device_addcat(struct device* parent, char* name_fmt, ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device* dev = device_alloc(parent, DEV_IFCAT, NULL);

    device_setname_vargs(dev, name_fmt, args);
    device_register(dev, NULL, NULL);

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

void
device_populate_info(struct device* dev, struct dev_info* devinfo)
{
    devinfo->dev_id.group = dev->ident.fn_grp;
    devinfo->dev_id.unique = dev->ident.unique;

    if (!devinfo->dev_name.buf) {
        return;
    }

    struct device_def* def = devdef_byident(&dev->ident);
    size_t buflen = devinfo->dev_name.buf_len;

    strncpy(devinfo->dev_name.buf, def->name, buflen);
    devinfo->dev_name.buf[buflen - 1] = 0;
}

struct device*
device_cast(void* obj)
{
    struct device* dev = (struct device*)obj;
    if (dev && dev->magic == DEV_STRUCT_MAGIC) {
        return dev;
    }

    return NULL;
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