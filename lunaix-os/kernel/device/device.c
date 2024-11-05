
#include <lunaix/device.h>
#include <lunaix/fs.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/ioctl.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

#include <klibc/strfmt.h>
#include <klibc/string.h>

static DEFINE_LLIST(root_list);

static volatile u32_t devid = 0;

struct devclass default_devclass = {};

void
device_setname_vargs(struct device_meta* dev, char* fmt, va_list args)
{
    size_t strlen = ksnprintfv(dev->name_val, fmt, DEVICE_NAME_SIZE, args);

    dev->name = HSTR(dev->name_val, strlen);

    hstr_rehash(&dev->name, HSTR_FULL_HASH);
}

void
device_register_generic(struct device_meta* devm, struct devclass* class, char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (fmt) {
        device_setname_vargs(devm, fmt, args);
    }

    if (class && valid_device_subtype_ref(devm, DEV_STRUCT)) {
        struct device* dev = to_dev(devm);
        dev->ident = (struct devident){ .fn_grp = class->fn_grp,
                                        .unique = DEV_UNIQUE(class->device,
                                                             class->variant) };
    }

    devm->dev_uid = devid++;

    struct device_meta* parent = devm->parent;
    if (parent) {
        assert(valid_device_subtype_ref(parent, DEV_CAT));
        llist_append(&parent->children, &devm->siblings);
    } else {
        llist_append(&root_list, &devm->siblings);
    }

    va_end(args);
}

static void
device_init_meta(struct device_meta* dmeta, struct device_meta* parent, unsigned int subtype) 
{
    dmeta->magic = DEV_STRUCT_MAGIC_MASK | subtype;
    dmeta->parent = parent;

    llist_init_head(&dmeta->children);
}

void
device_create(struct device* dev,
              struct device_meta* parent,
              u32_t type,
              void* underlay)
{
    dev->magic = DEV_STRUCT_MAGIC;
    dev->underlay = underlay;
    dev->dev_type = type;

    device_init_meta(dev_meta(dev), parent, DEV_STRUCT);
    llist_init_head(&dev->potentium);
    mutex_init(&dev->lock);
    iopoll_init_evt_q(&dev->pollers);
}

struct device*
device_alloc(struct device_meta* parent, u32_t type, void* underlay)
{
    struct device* dev = vzalloc(sizeof(struct device));

    if (!dev) {
        return NULL;
    }

    device_create(dev, parent, type, underlay);

    return dev;
}

struct device_alias*
device_alloc_alias(struct device_meta* parent, struct device_meta* aliased)
{
    struct device_alias* dev = vzalloc(sizeof(struct device_alias));

    if (!dev) {
        return NULL;
    }

    device_init_meta(dev_meta(dev), parent, DEV_ALIAS);
    dev->alias = aliased;

    return dev;
}

struct device_cat*
device_alloc_cat(struct device_meta* parent)
{
    struct device_cat* dev = vzalloc(sizeof(struct device_cat));

    if (!dev) {
        return NULL;
    }

    device_init_meta(dev_meta(dev), parent, DEV_CAT);

    return dev;
}


void
device_setname(struct device_meta* dev, char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    device_setname_vargs(dev, fmt, args);

    va_end(args);
}

struct device_cat*
device_addcat(struct device_meta* parent, char* name_fmt, ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device_cat* dev = device_alloc_cat(parent);

    device_setname_vargs(dev_meta(dev), name_fmt, args);
    device_register_generic(dev_meta(dev), NULL, NULL);

    va_end(args);
    return dev;
}

struct device_alias*
device_addalias(struct device_meta* parent, struct device_meta* aliased, char* name_fmt, ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device_alias* dev = device_alloc_alias(parent, aliased);

    device_setname_vargs(dev_meta(dev), name_fmt, args);
    device_register_generic(dev_meta(dev), NULL, NULL);

    va_end(args);
    return dev;
}

struct device_meta*
device_getbyid(struct llist_header* devlist, u32_t id)
{
    devlist = devlist ? devlist : &root_list;
    struct device_meta *pos, *n;
    llist_for_each(pos, n, devlist, siblings)
    {
        if (pos->dev_uid == id) {
            return pos;
        }
    }

    return NULL;
}

struct device_meta*
device_getbyhname(struct device_meta* root_dev, struct hstr* name)
{
    struct llist_header* devlist = root_dev ? &root_dev->children : &root_list;
    struct device_meta *pos, *n;
    llist_for_each(pos, n, devlist, siblings)
    {
        if (HSTR_EQ(&pos->name, name)) {
            return pos;
        }
    }

    return NULL;
}

struct device_meta*
device_getbyname(struct device_meta* root_dev, const char* name, size_t len)
{
    struct hstr hname = HSTR(name, len);
    hstr_rehash(&hname, HSTR_FULL_HASH);

    return device_getbyhname(root_dev, &hname);
}

void
device_remove(struct device_meta* dev)
{
    llist_delete(&dev->siblings);
    vfree(dev);
}

struct device_meta*
device_getbyoffset(struct device_meta* root_dev, int offset)
{
    struct llist_header* devlist = root_dev ? &root_dev->children : &root_list;
    struct device_meta *pos, *n;
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

struct device_meta*
resolve_device_meta(void* maybe_dev) {
    if (!valid_device_ref(maybe_dev)) {
        return NULL;
    }

    struct device_meta* dm = (struct device_meta*)maybe_dev;
    unsigned int subtype = dm->magic ^ DEV_STRUCT_MAGIC_MASK;
    
    switch (subtype)
    {
        case DEV_STRUCT:
        case DEV_CAT:
            return dm;
        
        case DEV_ALIAS: {
            struct device_meta* aliased_dm = dm;
            
            while(valid_device_subtype_ref(aliased_dm, DEV_ALIAS)) {
                aliased_dm = to_aliasdev(aliased_dm)->alias;
            }

            return aliased_dm;
        }
        default:
            return NULL;
    }
}

struct device*
resolve_device(void* maybe_dev) {
    struct device_meta* dm = resolve_device_meta(maybe_dev);
    
    if (!valid_device_subtype_ref(dm, DEV_STRUCT)) {
        return NULL;
    }

    return to_dev(dm);
}

void
device_alert_poller(struct device* dev, int poll_evt)
{
    dev->poll_evflags = poll_evt;
    iopoll_wake_pollers(&dev->pollers);
}

void
device_chain_loader(struct device_def* def, devdef_ldfn fn)
{
    struct device_ldfn_chain* node;

    node = valloc(sizeof(*node));
    node->load = fn;

    if (!def->load_chain) {
        node->chain = NULL;
    }
    else {
        node->chain = def->load_chain;
    }

    def->load_chain = node;
}

void
device_chain_load_once(struct device_def* def)
{
    struct device_ldfn_chain *node, *next;

    if (def->load) {
        def->load(def);
    }

    node = def->load_chain;
    def->load_chain = NULL;
    
    while (node)
    {
        node->load(def);
        next = node->chain;
        vfree(node);

        node = next;
    }

    if (def->flags.no_default_realm) {
        return;
    }

    if (!def->create) {
        return;
    }

    def->create(def, NULL);
    
}

__DEFINE_LXSYSCALL3(int, ioctl, int, fd, int, req, sc_va_list, _args)
{
    int errno = -1;
    struct v_fd* fd_s;
    va_list args;

    convert_valist(&args, _args);

    if ((errno &= vfs_getfd(fd, &fd_s))) {
        goto done;
    }

    struct device* dev = resolve_device(fd_s->file->inode->data);
    if (!valid_device_subtype_ref(dev, DEV_STRUCT)) {
        errno = ENODEV;
        goto done;
    }

    if (req == DEVIOIDENT) {
        struct dev_info* devinfo = va_arg(args, struct dev_info*);
        device_populate_info(dev, devinfo);
        errno = 0;
    }

    if (!dev->ops.exec_cmd) {
        errno = ENOTSUP;
        goto done;
    }

    errno = dev->ops.exec_cmd(dev, req, args);

done:
    return DO_STATUS_OR_RETURN(errno);
}