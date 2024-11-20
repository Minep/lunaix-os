#include <lunaix/device.h>
#include <lunaix/fs.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/ioctl.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>
#include <lunaix/owloysius.h>

#include <klibc/strfmt.h>
#include <klibc/string.h>

static DEFINE_LLIST(root_list);

morph_t* device_mobj_root;

void
device_setname_vargs(struct device_meta* dev, char* fmt, va_list args)
{
    ksnprintfv(dev->name_val, fmt, DEVICE_NAME_SIZE, args);
    changeling_setname(dev_mobj(dev), dev->name_val);
}

void
device_register_generic(struct device_meta* devm, struct devclass* class, 
                        char* fmt, ...)
{
    va_list args;
    morph_t* morphed, *parent;

    morphed = &devm->mobj;
    va_start(args, fmt);

    if (fmt) {
        device_setname_vargs(devm, fmt, args);
    }

    if (class && morph_type_of(morphed, device_morpher)) 
    {
        struct device* dev = to_dev(devm);
        dev->ident = (struct devident) { 
            .fn_grp = class->fn_grp,
            .unique = DEV_UNIQUE(class->device, class->variant) 
        };
    }

    parent = morphed->parent;
    if (parent) {
        changeling_attach(parent, morphed);
    }

    va_end(args);
}

void
device_create(struct device* dev, struct device_meta* parent,
              u32_t type, void* underlay)
{
    dev->underlay = underlay;
    dev->dev_type = type;

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
    changeling_morph(dev_morph(parent), dev->mobj, NULL, device_morpher);

    return dev;
}

struct device_alias*
device_alloc_alias(struct device_meta* parent, struct device_meta* aliased)
{
    struct device_alias* dev = vzalloc(sizeof(struct device_alias));

    if (!dev) {
        return NULL;
    }

    dev->alias = aliased;
    changeling_ref(dev_mobj(aliased));
    changeling_morph(dev_morph(parent), dev->mobj, NULL, devalias_morpher);

    return dev;
}

struct device_cat*
device_alloc_cat(struct device_meta* parent)
{
    struct device_cat* dev = vzalloc(sizeof(struct device_cat));

    if (!dev) {
        return NULL;
    }

    changeling_morph(dev_morph(parent), dev->mobj, NULL, devcat_morpher);

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
device_addalias(struct device_meta* parent, 
                struct device_meta* aliased, char* name_fmt, ...)
{
    va_list args;
    va_start(args, name_fmt);

    struct device_alias* dev = device_alloc_alias(parent, aliased);

    device_setname_vargs(dev_meta(dev), name_fmt, args);
    device_register_generic(dev_meta(dev), NULL, NULL);

    va_end(args);
    return dev;
}

void
device_remove(struct device_meta* dev)
{
    changeling_isolate(&dev->mobj);
    vfree(dev);
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

morph_t*
resolve_device_morph(void* maybe_dev)
{
    struct device_alias* da = NULL;
    morph_t *morphed;
    
    morphed = morphed_ptr(maybe_dev);

    if (!is_changeling(morphed)) {
        return NULL;
    }

    if (morph_type_of(morphed, device_morpher)) 
    {
        return morphed;
    }

    if (morph_type_of(morphed, devcat_morpher)) 
    {
        return morphed;
    }

    while(morph_type_of(morphed, devalias_morpher)) 
    {
        da = changeling_reveal(morphed, devalias_morpher);
        morphed = &da->alias->mobj;
    }

    return da ? morphed : NULL;
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
    if (!dev) {
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

static void
__device_subsys_init()
{
    device_mobj_root = changeling_spawn(NULL, "devices");
}
owloysius_fetch_init(__device_subsys_init, on_sysconf);