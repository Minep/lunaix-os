#include <klibc/stdio.h>
#include <lunaix/device.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/mm/valloc.h>

struct llist_header dev_list;

static struct twifs_node* dev_root;

int
__dev_read(struct v_inode* inode, void* buffer, size_t len, size_t fpos);

int
__dev_write(struct v_inode* inode, void* buffer, size_t len, size_t fpos);

void
device_init()
{
    dev_root = twifs_toplevel_node("dev", 3, 0);

    llist_init_head(&dev_list);
}

struct device*
__device_add(struct device* parent,
             void* underlay,
             char* name_fmt,
             uint32_t type,
             va_list args)
{
    struct device* dev = vzalloc(sizeof(struct device));

    size_t strlen =
      __sprintf_internal(dev->name_val, name_fmt, DEVICE_NAME_SIZE, args);

    dev->name = HSTR(dev->name_val, strlen);
    dev->parent = parent;
    dev->underlay = underlay;

    hstr_rehash(&dev->name, HSTR_FULL_HASH);
    llist_append(&dev_list, &dev->dev_list);

    struct twifs_node* dev_node =
      twifs_file_node(dev_root, dev->name_val, strlen, type);
    dev_node->data = dev;
    dev_node->ops.read = __dev_read;
    dev_node->ops.write = __dev_write;

    dev->fs_node = dev_node;

    return dev;
}

struct device*
device_addseq(struct device* parent, void* underlay, char* name_fmt, ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device* dev =
      __device_add(parent, underlay, name_fmt, VFS_IFSEQDEV, args);

    va_end(args);
    return dev;
}

struct device*
device_addvol(struct device* parent, void* underlay, char* name_fmt, ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device* dev =
      __device_add(parent, underlay, name_fmt, VFS_IFVOLDEV, args);

    va_end(args);
    return dev;
}

int
__dev_read(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    struct twifs_node* dev_node = (struct twifs_node*)inode->data;
    struct device* dev = (struct device*)dev_node->data;

    if (!dev->read) {
        return ENOTSUP;
    }
    return dev->read(dev, buffer, fpos, len);
}

int
__dev_write(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    struct twifs_node* dev_node = (struct twifs_node*)inode->data;
    struct device* dev = (struct device*)dev_node->data;

    if (!dev->write) {
        return ENOTSUP;
    }
    return dev->write(dev, buffer, fpos, len);
}

void
device_remove(struct device* dev)
{
    llist_delete(&dev->dev_list);
    twifs_rm_node((struct twifs_node*)dev->fs_node);
    vfree(dev);
}