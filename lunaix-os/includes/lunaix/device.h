#ifndef __LUNAIX_DEVICE_H
#define __LUNAIX_DEVICE_H

#define DEVICE_NAME_SIZE 32

#include <lunaix/device_num.h>
#include <lunaix/ds/hashtable.h>
#include <lunaix/ds/hstr.h>
#include <lunaix/ds/ldga.h>
#include <lunaix/ds/llist.h>
#include <lunaix/ds/semaphore.h>
#include <lunaix/types.h>

/**
 * @brief Export a device definition
 *
 */
#define EXPORT_DEVICE(id, devdef, load_order)                                  \
    export_ldga_el(devdefs, id, ptr_t, devdef);                                \
    export_ldga_el_sfx(devdefs, id##_ldorder, ptr_t, devdef, load_order);

#define load_on_demand ld_ondemand

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
#define DEVCLASS(devif, devfn, devkind, devvar)                                \
    (struct devclass)                                                          \
    {                                                                          \
        .meta = DEV_META(devif, devfn), .device = (devkind),                   \
        .variant = (devvar)                                                    \
    }

#define DEV_STRUCT_MAGIC 0x5645444c

#define DEV_MSKIF 0x00000003

#define DEV_IFVOL 0x0 // volumetric (block) device
#define DEV_IFSEQ 0x1 // sequential (character) device
#define DEV_IFCAT 0x2 // a device category (as device groupping)
#define DEV_IFSYS 0x3 // a system device

struct devclass
{
    u32_t meta;
    u32_t device;
    u32_t variant;
    u32_t hash;
};

struct device
{
    u32_t magic;
    struct llist_header siblings;
    struct llist_header children;
    struct device* parent;
    // TODO investigate event polling

    struct hstr name;
    struct devclass* class;
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
    int (*init_for)(struct device_def*, struct device*);
};

static inline u32_t devclass_hash(struct devclass class)
{
    return (((class.device & 0xffff) << 16) | (class.variant & 0xffff)) ^
           ~class.meta;
}

static inline u32_t
device_id_from_class(struct devclass* class)
{
    return ((class->device & 0xffff) << 16) | ((class->variant & 0xffff));
}

void
device_register_all();

void
device_prepare(struct device* dev, struct devclass* class);

void
device_setname(struct device* dev, char* fmt, ...);

void
device_setname(struct device* dev, char* fmt, ...);

struct device*
device_add_vargs(struct device* parent,
                 void* underlay,
                 char* name_fmt,
                 u32_t type,
                 struct devclass* class,
                 va_list args);

struct device*
device_add(struct device* parent,
           struct devclass* class,
           void* underlay,
           u32_t type,
           char* name_fmt,
           ...);

struct device*
device_addsys(struct device* parent,
              struct devclass* class,
              void* underlay,
              char* name_fmt,
              ...);

struct device*
device_addseq(struct device* parent,
              struct devclass* class,
              void* underlay,
              char* name_fmt,
              ...);

struct device*
device_addvol(struct device* parent,
              struct devclass* class,
              void* underlay,
              char* name_fmt,
              ...);

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
device_create_byclass(struct devclass* class,
                      u32_t type,
                      char* name,
                      int* err_code);

struct hbucket*
device_definitions_byif(int if_type);

struct device_def*
devdef_byclass(struct devclass* class);

void
device_register_all();

/*------ Load hooks ------*/

void
device_earlystage();

void
device_poststage();

void
device_timerstage();

#endif /* __LUNAIX_DEVICE_H */
