#include <klibc/stdio.h>
#include <lunaix/device.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/mm/valloc.h>

struct llist_header dev_list;

static struct twifs_node* dev_root;

int
__dev_read(struct v_file* file, void* buffer, size_t len);

int
__dev_write(struct v_file* file, void* buffer, size_t len);

void
device_init()
{
    dev_root = twifs_toplevel_node("dev", 3);

    llist_init_head(&dev_list);
}

struct device*
device_add(struct device* parent, void* underlay, char* name_fmt, ...)
{
    struct device* dev = vzalloc(sizeof(struct device));

    va_list args;
    va_start(args, name_fmt);

    size_t strlen =
      __sprintf_internal(dev->name_val, name_fmt, DEVICE_NAME_SIZE, args);

    dev->name = HSTR(dev->name_val, strlen);
    dev->parent = parent;
    dev->underlay = underlay;

    hstr_rehash(&dev->name, HSTR_FULL_HASH);
    llist_append(&dev_list, &dev->dev_list);

    struct twifs_node* dev_node =
      twifs_file_node(dev_root, dev->name_val, strlen);
    dev_node->data = dev;
    dev_node->fops.read = __dev_read;
    dev_node->fops.write = __dev_write;

    dev->fs_node = dev_node;

    va_end(args);
    return dev;
}

int
__dev_read(struct v_file* file, void* buffer, size_t len)
{
    struct twifs_node* dev_node = (struct twifs_node*)file->inode->data;
    struct device* dev = (struct device*)dev_node->data;

    if (!dev->read) {
        return ENOTSUP;
    }
    return dev->read(dev, buffer, file->f_pos, len);
}

int
__dev_write(struct v_file* file, void* buffer, size_t len)
{
    struct twifs_node* dev_node = (struct twifs_node*)file->inode->data;
    struct device* dev = (struct device*)dev_node->data;

    if (!dev->write) {
        return ENOTSUP;
    }
    return dev->write(dev, buffer, file->f_pos, len);
}

void
device_remove(struct device* dev)
{
    llist_delete(&dev->dev_list);
    twifs_rm_node((struct twifs_node*)dev->fs_node);
    vfree(dev);
}