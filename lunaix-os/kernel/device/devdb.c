#include <lunaix/device.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/status.h>

#include <klibc/stdio.h>

static DECLARE_HASHTABLE(dev_registry, 32);
static DECLARE_HASHTABLE(dev_byif, 8);
static DEFINE_LLIST(dev_registry_flat);

static struct device* adhoc_devcat;

void
device_register_all()
{
    adhoc_devcat = device_addcat(NULL, "adhoc");

    hashtable_init(dev_registry);
    hashtable_init(dev_byif);

    int idx = 0;
    struct device_def* devdef;
    ldga_foreach(devdefs, struct device_def*, idx, devdef)
    {
        u32_t hash = devclass_hash(devdef->class);
        devdef->class.hash = hash;

        if (!devdef->name) {
            devdef->name = "<unspecified>";
        }

        hashtable_hash_in(dev_registry, &devdef->hlist, hash);
        hashtable_hash_in(
          dev_byif, &devdef->hlist_if, DEV_IF(devdef->class.meta));

        llist_append(&dev_registry_flat, &devdef->dev_list);
    }
}

static int
devclass_eq(struct devclass* c1, struct devclass* c2)
{
    return c1->meta == c2->meta && c1->variant == c2->variant &&
           c1->device == c2->device;
}

struct device_def*
devdef_byclass(struct devclass* class)
{
    u32_t hash = devclass_hash(*class);
    int errno;

    struct device_def *pos, *n;
    hashtable_hash_foreach(dev_registry, hash, pos, n, hlist)
    {
        if (pos->class.hash != hash) {
            continue;
        }
        if (devclass_eq(class, &pos->class)) {
            break;
        }
    }

    return pos;
}

struct device*
device_create_byclass(struct devclass* class,
                      u32_t type,
                      char* name,
                      int* err_code)
{
    int errno;
    struct device_def* devdef = devdef_byclass(class);

    if (!devdef) {
        *err_code = ENOENT;
        return NULL;
    }

    if (!devdef->init_for) {
        if (err_code) {
            *err_code = ENOTSUP;
        }
        return NULL;
    }

    struct device* dev = device_add(adhoc_devcat, class, NULL, type, NULL);

    errno = devdef->init_for(devdef, dev);
    if (err_code && !errno) {
        *err_code = errno;
        device_remove(dev);
        return NULL;
    }

    device_setname(dev,
                   "%s_%d:%d:%d_%d",
                   name,
                   class->meta,
                   class->device,
                   class->device,
                   dev->dev_uid);

    return dev;
}

struct hbucket*
device_definitions_byif(int if_type)
{
    return &dev_byif[__hashkey(dev_byif, if_type)];
}

#define device_load_on_stage(stage)                                            \
    ({                                                                         \
        int idx = 0;                                                           \
        struct device_def* devdef;                                             \
        ldga_foreach(dev_ld_##stage, struct device_def*, idx, devdef)          \
        {                                                                      \
            devdef->init(devdef);                                              \
        }                                                                      \
    })

void
device_earlystage()
{
    device_load_on_stage(early);
}

void
device_timerstage()
{
    device_load_on_stage(aftertimer);
}

void
device_poststage()
{
    device_load_on_stage(post);
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

    int meta = def->class.meta;
    ksnprintf(flags, 64, "if=%x,fn=%x", DEV_IF(meta), DEV_FN(meta));

    twimap_printf(mapping,
                  "%xh:%d:%d \"%s\" %s\n",
                  def->class.meta,
                  def->class.device,
                  def->class.variant,
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