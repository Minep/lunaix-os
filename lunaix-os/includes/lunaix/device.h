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

#define DEV_STRUCT_MAGIC 0x5645444c

#define DEV_MSKIF 0x00000003

#define DEV_IFVOL 0x0 // volumetric (block) device
#define DEV_IFSEQ 0x1 // sequential (character) device
#define DEV_IFCAT 0x2 // a device category (as device groupping)
#define DEV_IFSYS 0x3 // a system device

struct device
{
    /* -- device structing -- */

    u32_t magic;
    struct llist_header siblings;
    struct llist_header children;
    struct device* parent;

    /* -- device state -- */

    mutex_t lock;

    struct hstr name;
    struct devident ident;

    u32_t dev_uid;
    int dev_type;
    char name_val[DEVICE_NAME_SIZE];
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
device_onbooot_load();

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
