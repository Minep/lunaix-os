#ifndef __LUNAIX_DEVICE_H
#define __LUNAIX_DEVICE_H

#define DEVICE_NAME_SIZE 32

#include <lunaix/device_num.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/ds/hstr.h>
#include <lunaix/ds/ldga.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/mutex.h>
#include <lunaix/iopoll.h>
#include <lunaix/types.h>

#include <usr/lunaix/device.h>

/**
 * @brief Export a device definition (i.e., device driver metadata)
 *
 */
#define EXPORT_DEVICE(id, devdef, load_order)                                  \
    export_ldga_el(devdefs, id, ptr_t, devdef);                                \
    export_ldga_el_sfx(devdefs, id##_ldorder, ptr_t, devdef, load_order);

/**
 * @brief Mark the device definition can be loaded on demand, all other loading
 * options are extended from this
 */
#define load_on_demand ld_ondemand

/**
 * @brief Mark the device definition to be loaded as system configuration
 * device. These kind of devices are defined to be the devices that talk to the
 * system firmware to do config, or collecting crucial information about the
 * system. For instances, ACPI, SoC components, and other **interconnection**
 * buese (not USB!). Such device driver must only rely on basic memory
 * management service, and must not try accessing subsystems other than the mm
 * unit, for example, timer, interrupt, file-system, must not assumed exist.
 *
 */
#define load_sysconf ld_sysconf

/**
 * @brief Mark the device definition should be loaded as time device, for
 * example a real time clock device. Such device will be loaded and managed by
 * clock subsystem
 */
#define load_timedev ld_timedev

/**
 * @brief Mark the device definition should be loaded automatically during the
 * bootstrapping stage. Most of the driver do load there.
 *
 */
#define load_onboot ld_kboot

/**
 * @brief Mark the device definition should be loaded automatically in
 * the post boostrapping stage (i.e., the start up of proc0), where most of
 * kernel sub-system are became ready to use. Do your load there if your driver
 * depends on such condition
 *
 */
#define load_postboot ld_post

#define __foreach_exported_device_of(stage, index, pos)                        \
    ldga_foreach(dev_##stage, struct device_def*, index, pos)
#define foreach_exported_device_of(stage, index, pos)                          \
    __foreach_exported_device_of(stage, index, pos)

/**
 * @brief Declare a device class
 *
 */
#define DEVCLASS(devif, devfn, dev)                                            \
    (struct devclass)                                                          \
    {                                                                          \
        .fn_grp = DEV_FNGRP(devif, devfn), .device = (dev), .variant = 0       \
    }

#define DEVCLASSV(devif, devfn, dev, devvar)                                   \
    (struct devclass)                                                          \
    {                                                                          \
        .fn_grp = DEV_FNGRP(devif, devfn), .device = (dev),                    \
        .variant = (devvar)                                                    \
    }

#define DEV_STRUCT_MAGIC_MASK 0x56454440U
#define DEV_STRUCT 0xc
#define DEV_CAT 0xd
#define DEV_ALIAS 0xf

#define DEV_STRUCT_MAGIC (DEV_STRUCT_MAGIC_MASK | DEV_STRUCT)
#define DEV_CAT_MAGIC (DEV_STRUCT_MAGIC_MASK | DEV_CAT)
#define DEV_ALIAS_MAGIC (DEV_STRUCT_MAGIC_MASK | DEV_ALIAS)

#define DEV_MSKIF 0x00000003

#define DEV_IFVOL 0x0 // volumetric (block) device
#define DEV_IFSEQ 0x1 // sequential (character) device
#define DEV_IFSYS 0x3 // a system device

struct capability_meta
{
    struct llist_header caps;
    unsigned int cap_type;
};

#define CAPABILITY_META                     \
    union {                                 \
        struct capability_meta cap_meta;    \
        struct {                            \
            struct llist_header caps;       \
            unsigned int cap_type;          \
        };                                  \
    }

#define get_capability(cap, cap_type)       \
    container_of((cap), cap_type, cap_meta)
#define cap_meta(cap) (&(cap)->cap_meta)

typedef struct llist_header capability_list_t;


struct device_meta
{
    u32_t magic;
    struct llist_header siblings;
    struct llist_header children;
    struct device_meta* parent;
    struct hstr name;
    
    u32_t dev_uid;
    
    char name_val[DEVICE_NAME_SIZE];
};

#define DEVICE_METADATA                             \
    union {                                         \
        struct device_meta meta;                    \
        struct {                                    \
            u32_t magic;                            \
            struct llist_header siblings;           \
            struct llist_header children;           \
            struct device_meta* parent;             \
            struct hstr name;                       \
                                                    \
            u32_t dev_uid;                          \
                                                    \
            char name_val[DEVICE_NAME_SIZE];        \
        };                                          \
    }                                              

#define dev_meta(dev) (&(dev)->meta)
#define to_dev(dev) (container_of(dev,struct device, meta))
#define to_catdev(dev) (container_of(dev,struct device_cat, meta))
#define to_aliasdev(dev) (container_of(dev,struct device_alias, meta))

struct device_alias {
    DEVICE_METADATA;
    struct device_meta* alias;
};

struct device_cat {
    DEVICE_METADATA;
};

struct device
{
    /* -- device structing -- */

    DEVICE_METADATA;

    capability_list_t capabilities;

    /* -- device state -- */

    mutex_t lock;

    int dev_type;
    struct devident ident;
    void* underlay;

    /* -- polling -- */
    int poll_evflags;
    poll_evt_q pollers;

    struct
    {
        // TODO Think about where will they fit.
        int (*acquire)(struct device* dev);
        int (*release)(struct device* dev);

        int (*read)(struct device*, void*, off_t, size_t);
        int (*write)(struct device*, void*, off_t, size_t);
        int (*read_async)(struct device*, void*, off_t, size_t);
        int (*write_async)(struct device*, void*, off_t, size_t);

        int (*read_page)(struct device*, void*, off_t);
        int (*write_page)(struct device*, void*, off_t);

        int (*exec_cmd)(struct device*, u32_t, va_list);
        int (*poll)(struct device*);
    } ops;
};

struct device_def
{
    struct llist_header dev_list;
    struct hlist_node hlist;
    struct hlist_node hlist_if;
    char* name;

    struct devclass class;

    /**
     * @brief Called when the driver is required to initialize itself.
     *
     */
    int (*init)(struct device_def*);

    /**
     * @brief Called when the driver is required to bind with a device. This is
     * the case for a real-hardware-oriented driver
     *
     */
    int (*bind)(struct device_def*, struct device*);

    /**
     * @brief Called when a driver is requested to detach from the device and
     * free up all it's resources
     *
     */
    int (*free)(struct device_def*, void* instance);
};

static inline bool must_inline
valid_device_ref(void* maybe_dev) {
    if (!maybe_dev) 
        return false;
        
    unsigned int magic = ((struct device_meta*)maybe_dev)->magic;
    return (magic ^ DEV_STRUCT_MAGIC_MASK) <= 0xfU;
}

static inline bool must_inline
valid_device_subtype_ref(void* maybe_dev, unsigned int subtype) {
    if (!maybe_dev) 
        return false;
    
    unsigned int magic = ((struct device_meta*)maybe_dev)->magic;
    return (magic ^ DEV_STRUCT_MAGIC_MASK) == subtype;
}

struct device*
resolve_device(void* maybe_dev);

struct device_meta*
resolve_device_meta(void* maybe_dev);

#define mark_device_doing_write(dev_ptr) (dev_ptr)->poll_evflags &= ~_POLLOUT
#define mark_device_done_write(dev_ptr) (dev_ptr)->poll_evflags |= _POLLOUT

#define mark_device_doing_read(dev_ptr) (dev_ptr)->poll_evflags &= ~_POLLIN
#define mark_device_done_read(dev_ptr) (dev_ptr)->poll_evflags |= _POLLIN

#define mark_device_hanging(dev_ptr) (dev_ptr)->poll_evflags &= ~_POLLHUP
#define mark_device_grounded(dev_ptr) (dev_ptr)->poll_evflags |= _POLLHUP

static inline u32_t
device_id_from_class(struct devclass* class)
{
    return ((class->device & 0xffff) << 16) | ((class->variant & 0xffff));
}

void
device_scan_drivers();

void
device_setname_vargs(struct device_meta* dev, char* fmt, va_list args);

void
device_setname(struct device_meta* dev, char* fmt, ...);

void
device_register_generic(struct device_meta* dev, struct devclass* class, char* fmt, ...);

#define register_device(dev, class, fmt, ...) \
            device_register_generic(dev_meta(dev), class, fmt, ## __VA_ARGS__)

void
device_create(struct device* dev,
              struct device_meta* parent,
              u32_t type,
              void* underlay);

struct device*
device_alloc(struct device_meta* parent, u32_t type, void* underlay);

static inline struct device* must_inline
device_allocsys(struct device_meta* parent, void* underlay)
{
    return device_alloc(parent, DEV_IFSYS, underlay);
}

static inline struct device* must_inline
device_allocseq(struct device_meta* parent, void* underlay)
{
    return device_alloc(parent, DEV_IFSEQ, underlay);
}

static inline struct device* must_inline
device_allocvol(struct device_meta* parent, void* underlay)
{
    return device_alloc(parent, DEV_IFVOL, underlay);
}

struct device_alias*
device_addalias(struct device_meta* parent, struct device_meta* aliased, char* name_fmt, ...);

struct device_cat*
device_addcat(struct device_meta* parent, char* name_fmt, ...);

void
device_remove(struct device_meta* dev);

struct device_meta*
device_getbyid(struct llist_header* devlist, u32_t id);

struct device_meta*
device_getbyhname(struct device_meta* root_dev, struct hstr* name);

struct device_meta*
device_getbyname(struct device_meta* root_dev, const char* name, size_t len);

struct device_meta*
device_getbyoffset(struct device_meta* root_dev, int pos);

struct hbucket*
device_definitions_byif(int if_type);

struct device_def*
devdef_byident(struct devident* class);

void
device_populate_info(struct device* dev, struct dev_info* devinfo);

void
device_scan_drivers();

/*------ Capability ------*/

struct capability_meta*
alloc_capability(int cap, unsigned int size);

#define new_capability(cap_type, cap_impl)\
    ((cap_impl*)alloc_capability((cap_type), sizeof(cap_impl)))

#define new_capability_marker(cap_type)\
    (alloc_capability((cap_type), sizeof(struct capability_meta)))

void
device_grant_capability(struct device* dev, struct capability_meta* cap);

struct capability_meta*
device_get_capability(struct device* dev, unsigned int cap_type);
/*------ Load hooks ------*/

void
device_onboot_load();

void
device_postboot_load();

void
device_sysconf_load();

static inline void
device_lock(struct device* dev)
{
    mutex_lock(&dev->lock);
}

static inline void
device_unlock(struct device* dev)
{
    mutex_unlock(&dev->lock);
}

static inline int
device_locked(struct device* dev)
{
    return mutex_on_hold(&dev->lock);
}

#define devprintf_expand(devident) (devident)->fn_grp, (devident)->unique

#endif /* __LUNAIX_DEVICE_H */
