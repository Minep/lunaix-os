#ifndef __LUNAIX_DEVICE_H
#define __LUNAIX_DEVICE_H

#define DEVICE_NAME_SIZE 16

#include <lunaix/device_num.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/ds/hstr.h>
#include <lunaix/ds/ldga.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/mutex.h>
#include <lunaix/iopoll.h>
#include <lunaix/types.h>
#include <lunaix/changeling.h>

#include <hal/devtreem.h>

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
#define DEVCLASS(vendor, devfn, dev)                                           \
    (struct devclass)                                                          \
    {                                                                          \
        .fn_grp = DEV_FNGRP(dev_vn(vendor), dev_fn(devfn)),                    \
        .device = dev_id(dev), .variant = 0                                    \
    }

#define DEV_MSKIF 0x00000003
#define DEV_IFVOL 0x0 // volumetric (block) device
#define DEV_IFSEQ 0x1 // sequential (character) device
#define DEV_IFSYS 0x3 // a system device

#define dev_object_root         \
        ({ extern morph_t* device_mobj_root; device_mobj_root; })

/**
 * A potens is a capability of the device
 *      ("potens", means "capable" in latin)
 * 
 * A device can have multiple capabilities 
 *      (or "potentes", plural form of potens)
 * 
 * A group of devices with same capability will forms
 *  a "globus potentis" or "capability group". The
 *  group is completely a logical formation, meaning
 *  that it is not explictly coded.
 * 
 * The idea of potens is to provide a unified and yet
 *  opaque method to decouple the device provide raw
 *  functionalities with any higher abstraction, such
 *  as other subsystem, or standard-compilance wrapper
 *  (e.g., POSIX terminal)
 */
struct potens_meta
{
    struct device* owner;
    
    struct llist_header potentes;
    unsigned int pot_type;
};

#define POTENS_META                                         \
    struct {                                                \
        union {                                             \
            struct potens_meta pot_meta;                    \
            struct {                                        \
                struct llist_header potentes;               \
                unsigned int pot_type;                      \
            };                                              \
        };                                                  \
    }

#define get_potens(cap, pot_struct)       \
            container_of((cap), pot_struct, pot_meta)
#define potens_meta(cap) (&(cap)->pot_meta)
#define potens_dev(cap) (potens_meta(cap)->owner)

#define potens(name)   POTENS_##name

#define potens_check_unique(dev, pot_type)                   \
            !device_get_potens(dev, pot_type)

enum device_potens_type
{
    potens(NON),
    #include <listings/device_potens.lst>
};

/**
 * List of potentes of a device, pay attention to 
 * the spelling, "potentium", genitive plural of "potens".
 */
typedef struct llist_header potentium_list_t;


#define DEVICE_META_FIELD                       \
    struct {                                    \
        morph_t mobj;                           \
        char name_val[DEVICE_NAME_SIZE];        \
    };

#define DEVICE_METADATA                             \
    union {                                         \
        struct device_meta meta;                    \
        DEVICE_META_FIELD;                          \
    }   

#define devmeta_morpher      morphable_attrs(device_meta, mobj)
#define devalias_morpher     morphable_attrs(device_alias, mobj)
#define devcat_morpher       morphable_attrs(device_cat, mobj)
#define device_morpher       morphable_attrs(device, mobj)

#define dev_meta(dev)       (&(dev)->meta)
#define dev_mobj(dev)       (&(dev)->mobj)
#define dev_morph(dev)      ({ likely(dev) ? &(dev)->mobj : dev_object_root; })
#define dev_uid(dev)        (morpher_uid(&(dev)->mobj))
#define to_dev(dev)         (container_of(dev,struct device, meta))
#define to_catdev(dev)      (container_of(dev,struct device_cat, meta))
#define to_aliasdev(dev)    (container_of(dev,struct device_alias, meta))

struct device_meta
{
    DEVICE_META_FIELD;
};

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

    potentium_list_t potentium;

#ifdef CONFIG_USE_DEVICETREE
    devtree_link_t devtree_node;
#endif

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

struct device_def;
typedef int (*devdef_ldfn)(struct device_def*);

struct device_ldfn_chain
{
    struct device_ldfn_chain* chain;
    devdef_ldfn load;
};

struct device_def
{
    struct llist_header dev_list;
    struct hlist_node hlist;
    struct hlist_node hlist_if;
    char* name;

    union
    {
        struct {
            bool no_default_realm : 1;
        };
        int val;
    } flags;
    

    struct {
        struct devclass class;
        unsigned int class_hash;
    };

    struct device_ldfn_chain* load_chain;

    /**
     * @brief 
     * Called when driver is required to register itself to the system
     * All registration code should put it here.
     * 
     * ad tabulam -  
     *      "to the record" in latin. just in case anyone wonders.
     *      I am ran out of naming idea... have to improvise :)
     *
     */
    devdef_ldfn ad_tabulam;

    /**
     * @brief Called when the driver is loaded at it's desired load stage
     *
     */
    devdef_ldfn load;

    /**
     * @brief Called when the driver is required to create device instance
     * This is for device with their own preference of creation
     * object that hold parameter for such creation is provided by second argument.
     */
    int (*create)(struct device_def*, morph_t*);

    /**
     * @brief Called when a driver is requested to detach from the device and
     * free up all it's resources
     *
     */
    int (*free)(struct device_def*, void* instance);
};

#define def_device_name(dev_n)          .name = dev_n
#define def_device_class(_if, fn, dev)  .class = DEVCLASS(_if, fn, dev)
#define def_on_register(fn)             .ad_tabulam = fn
#define def_on_load(fn)                 .load = fn
#define def_on_create(fn)               .create = fn
#define def_on_free(fn)                 .free = fn
#define def_non_trivial                 .flags.no_default_realm = true

morph_t*
resolve_device_morph(void* maybe_dev);

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

static inline struct device*
resolve_device(void* maybe_dev) {
    morph_t* mobj = resolve_device_morph(maybe_dev);
    return changeling_try_reveal(mobj, device_morpher);
}

void
device_scan_drivers();

void
device_setname_vargs(struct device_meta* dev, char* fmt, va_list args);

void
device_setname(struct device_meta* dev, char* fmt, ...);

void
device_register_generic(struct device_meta* dev, struct devclass* class, 
                        char* fmt, ...);

#define register_device(dev, class, fmt, ...) \
            device_register_generic(dev_meta(dev), class, fmt, ## __VA_ARGS__)

#define register_device_var(dev, class, fmt, ...) \
            ({device_register_generic(                                     \
                    dev_meta(dev), class,                                  \
                    fmt "%d", ## __VA_ARGS__, (class)->variant);           \
              devclass_mkvar(class); })

void
device_create(struct device* dev,
              struct device_meta* parent,
              u32_t type,
              void* underlay);

struct device*
device_alloc(struct device_meta* parent, u32_t type, void* underlay);

static inline void
device_set_devtree_node(struct device* dev, devtree_link_t node)
{
    dev->devtree_node = node;
}

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
device_addalias(struct device_meta* parent, struct device_meta* aliased, 
                char* name_fmt, ...);

struct device_cat*
device_addcat(struct device_meta* parent, char* name_fmt, ...);

void
device_remove(struct device_meta* dev);

struct hbucket*
device_definitions_byif(int if_type);

struct device_def*
devdef_byident(struct devident* class);

void
device_populate_info(struct device* dev, struct dev_info* devinfo);

void
device_scan_drivers();

/*------ Capability ------*/

struct potens_meta*
alloc_potens(int cap, unsigned int size);

#define new_potens(pot_type, pot_struct)\
    ((pot_struct*)alloc_potens((pot_type), sizeof(pot_struct)))

#define new_potens_marker(pot_type)\
    (alloc_potens((pot_type), sizeof(struct potens_meta)))

void
device_grant_potens(struct device* dev, struct potens_meta* cap);

struct potens_meta*
device_get_potens(struct device* dev, unsigned int pot_type);

/*------ Load hooks ------*/

void
device_onboot_load();

void
device_postboot_load();

void
device_sysconf_load();

/**
 * @brief Add the loader to the chain, used by device domain
 *        to inject their custom loading logic to the hook
 */
void
device_chain_loader(struct device_def* def, devdef_ldfn fn);

/**
 * @brief Walk the chain and load in a use-and-burnt fashion.
 *        the chain will be deleted and freed after loading,
 *        regardless successful or not.
 */
void
device_chain_load_once(struct device_def* def);

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


/**
 * 
 * Device def hooks extern
 * 
 */

static int
default_onregister_hook(struct device_def* def)
{
    return 0;
}

static int
default_onload_hook(struct device_def* def)
{
    return 0;
}

static int
default_oncreate_hook(struct device_def* def, morph_t* morphed)
{
    return 0;
}

#define extern_hook_register(name)  \
            int weak_alias("default_onregister_hook") \
                name(struct device_def* def)

#define extern_hook_load(name)  \
            int weak_alias("default_onload_hook") \
                name(struct device_def* def)

#define extern_hook_create(name)  \
            int weak_alias("default_oncreate_hook") \
                name(struct device_def* def, morph_t* morphed)


#endif /* __LUNAIX_DEVICE_H */
