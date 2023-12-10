#include <lunaix/device.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/status.h>

#include <lib/hash.h>

#include <klibc/strfmt.h>

static DECLARE_HASHTABLE(dev_registry, 32);
static DECLARE_HASHTABLE(dev_byif, 8);
static DEFINE_LLIST(dev_registry_flat);

static struct device_cat* adhoc_devcat;

static inline u32_t
hash_dev(u32_t fngrp, u32_t dev)
{
    return (hash_32(fngrp, 16) << 16) | (hash_32(dev, 16));
}

void
device_scan_drivers()
{
    adhoc_devcat = device_addcat(NULL, "adhoc");

    hashtable_init(dev_registry);
    hashtable_init(dev_byif);

    int idx = 0;
    struct device_def* devdef;
    ldga_foreach(devdefs, struct device_def*, idx, devdef)
    {
        struct devclass* devc = &devdef->class;
        u32_t hash = hash_dev(devc->fn_grp, devc->device);
        devc->hash = hash;

        if (!devdef->name) {
            devdef->name = "<unspecified>";
        }

        hashtable_hash_in(dev_registry, &devdef->hlist, hash);
        hashtable_hash_in(dev_byif, &devdef->hlist_if, DEV_IF(devc->fn_grp));

        llist_append(&dev_registry_flat, &devdef->dev_list);
    }
}

static int
devclass_eq(struct devclass* c1, struct devclass* c2)
{
    return c1->fn_grp == c2->fn_grp && c1->device == c2->device;
}

struct device_def*
devdef_byclass(struct devclass* devc)
{
    u32_t hash = hash_dev(devc->fn_grp, devc->device);
    int errno;

    struct device_def *pos, *n;
    hashtable_hash_foreach(dev_registry, hash, pos, n, hlist)
    {
        if (pos->class.hash != hash) {
            continue;
        }
        if (devclass_eq(devc, &pos->class)) {
            break;
        }
    }

    return pos;
}

struct device_def*
devdef_byident(struct devident* ident)
{
    struct devclass derived = { .device = DEV_KIND_FROM(ident->unique),
                                .fn_grp = ident->fn_grp };
    return devdef_byclass(&derived);
}

struct hbucket*
device_definitions_byif(int if_type)
{
    return &dev_byif[__hashkey(dev_byif, if_type)];
}

#define __device_load_on_stage(stage)                                          \
    ({                                                                         \
        int idx = 0;                                                           \
        struct device_def* devdef;                                             \
        ldga_foreach(dev_##stage, struct device_def*, idx, devdef)             \
        {                                                                      \
            devdef->init(devdef);                                              \
        }                                                                      \
    })
#define device_load_on_stage(stage) __device_load_on_stage(stage)

void
device_onboot_load()
{
    device_load_on_stage(load_onboot);
}

void
device_postboot_load()
{
    device_load_on_stage(load_postboot);
}

void
device_sysconf_load()
{
    device_load_on_stage(load_sysconf);
}

static int
__devdb_db_gonext(struct twimap* mapping)
{
    struct device_def* current = twimap_index(mapping, struct device_def*);
    if (current->dev_list.next == &dev_registry_flat) {
        return 0;
    }
    mapping->index =
      list_entry(current->dev_list.next, struct device_def, dev_list);
    return 1;
}

static void
__devdb_twifs_lsdb(struct twimap* mapping)
{
    char flags[64];
    struct device_def* def = twimap_index(mapping, struct device_def*);

    int meta = def->class.fn_grp;
    ksnprintf(flags, 64, "if=%x,fn=%x", DEV_IF(meta), DEV_FN(meta));

    twimap_printf(mapping,
                  "%08xh:%04d \"%s\" %s\n",
                  def->class.fn_grp,
                  def->class.device,
                  def->name,
                  flags);
}

void
__devdb_reset(struct twimap* map)
{
    map->index =
      container_of(dev_registry_flat.next, struct device_def, dev_list);
}

static void
devdb_twifs_plugin()
{
    struct twimap* map = twifs_mapping(NULL, NULL, "devtab");
    map->reset = __devdb_reset;
    map->read = __devdb_twifs_lsdb;
    map->go_next = __devdb_db_gonext;
}
EXPORT_TWIFS_PLUGIN(devdb, devdb_twifs_plugin);