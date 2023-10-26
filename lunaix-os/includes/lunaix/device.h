#ifndef __LUNAIX_DEVICE_H
#define __LUNAIX_DEVICE_H

#define DEVICE_NAME_SIZE 32

#include <lunaix/device_num.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/ds/hstr.h>
#include <lunaix/ds/ldga.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/mutex.h>
#include <lunaix/types.h>

#include <usr/lunaix/device.h>

/**
 * @brief Export a device definition
 *
 */
#define EXPORT_DEVICE(id, devdef, load_order)                                  \
    export_ldga_el(devdefs, id, ptr_t, devdef);                                \
    export_ldga_el_sfx(devdefs, id##_ldorder, ptr_t, devdef, load_order);

#define load_on_demand ld_ondemand
#define load_pci_probe ld_ondemand

/**
 * @brief Mark the device definition should be loaded automatically as earlier
 * as possible in the kernel bootstrapping stage (before initialization of file
 * systems). Load here if your driver is standalone and require no other than
 * basic memory allocation services
 *
 */
#define load_earlystage ld_early

/**
 * @brief Mark the device definition should be loaded automatically after timer
 * is ready. Load here if your driver require a basic timing service
 *
 */
#define load_timerstage ld_aftertimer

/**
 * @brief Mark the device definition should be loaded automatically in
 * the post boostrapping stage (i.e., the start up of proc0). Load here if your
 * driver involves async mechanism
 *
 */
#define load_poststage ld_post

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

#define DEV_STRUCT_MAGIC 0x5645444c

#define DEV_MSKIF 0x00000003

#define DEV_IFVOL 0x0 // volumetric (block) device
#define DEV_IFSEQ 0x1 // sequential (character) device
#define DEV_IFCAT 0x2 // a device category (as device groupping)
#define DEV_IFSYS 0x3 // a system device

struct device
{
    u32_t magic;
    struct llist_header siblings;
    struct llist_header children;
    struct device* parent;
    mutex_t lock;

    // TODO investigate event polling

    struct hstr name;
    struct devident ident;

    u32_t dev_uid;
    int dev_type;
    char name_val[DEVICE_NAME_SIZE];
    void* underlay;

    struct
    {
        // TODO Think about where will they fit.
        int (*acquire)(struct device* dev);
        int (*release)(struct device* dev);

        int (*read)(struct device* dev, void* buf, size_t offset, size_t len);
        int (*write)(struct device* dev, void* buf, size_t offset, size_t len);
        int (*read_page)(struct device* dev, void* buf, size_t offset);
        int (*write_page)(struct device* dev, void* buf, size_t offset);
        int (*exec_cmd)(struct device* dev, u32_t req, va_list args);
    } ops;
};

struct device_def
{
    struct llist_header dev_list;
    struct hlist_node hlist;
    struct hlist_node hlist_if;
    char* name;

    struct devclass class;

    int (*init)(struct device_def*);
    int (*bind)(struct device_def*, struct device*);
    int (*free)(struct device_def*, void* instance);
};

static inline u32_t
device_id_from_class(struct devclass* class)
{
    return ((class->device & 0xffff) << 16) | ((class->variant & 0xffff));
}

void
device_scan_drivers();

void
device_setname_vargs(struct device* dev, char* fmt, va_list args);

void
device_setname(struct device* dev, char* fmt, ...);

void
device_register(struct device* dev, struct devclass* class, char* fmt, ...);

void
device_create(struct device* dev,
              struct device* parent,
              u32_t type,
              void* underlay);

struct device*
device_alloc(struct device* parent, u32_t type, void* underlay);

static inline struct device*
device_allocsys(struct device* parent, void* underlay)
{
    return device_alloc(parent, DEV_IFSYS, underlay);
}

static inline struct device*
device_allocseq(struct device* parent, void* underlay)
{
    return device_alloc(parent, DEV_IFSEQ, underlay);
}

static inline struct device*
device_allocvol(struct device* parent, void* underlay)
{
    return device_alloc(parent, DEV_IFVOL, underlay);
}

struct device*
device_addcat(struct device* parent, char* name_fmt, ...);

void
device_remove(struct device* dev);

struct device*
device_getbyid(struct llist_header* devlist, u32_t id);

struct device*
device_getbyhname(struct device* root_dev, struct hstr* name);

struct device*
device_getbyname(struct device* root_dev, const char* name, size_t len);

struct device*
device_getbyoffset(struct device* root_dev, int pos);

struct device*
device_cast(void* obj);

struct hbucket*
device_definitions_byif(int if_type);

struct device_def*
devdef_byident(struct devident* class);

void
device_populate_info(struct device* dev, struct dev_info* devinfo);

void
device_scan_drivers();

/*------ Load hooks ------*/

void
device_earlystage();

void
device_poststage();

void
device_timerstage();

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
