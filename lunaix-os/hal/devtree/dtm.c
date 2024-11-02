#include <hal/devtreem.h>
#include <lunaix/device.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/syslog.h>
#include <lunaix/owloysius.h>

#include <klibc/string.h>

LOG_MODULE("dtm")

static DECLARE_HASHTABLE(registry,    32);
static struct dtm_vendor_bag generic_drvs;
static struct device_cat* dt_category;

#define locate_entry(type, table, h_key, field)                                 \
    ({                                                                          \
        type *pos, *n, *found;                                                  \
                                                                                \
        found = NULL;                                                           \
        hashtable_hash_foreach(table, HSTR_HASH(h_key), pos, n, peers)          \
        {                                                                       \
            if (HSTR_EQ(&pos->field, &h_key))                                   \
            {                                                                   \
                found = pos;                                                    \
                break;                                                          \
            }                                                                   \
        }                                                                       \
        found;                                                                  \
    })

static struct dtm_vendor_bag*
__locate_vendor(struct hstr* vendor)
{
    return locate_entry(struct dtm_vendor_bag, registry, *vendor, vendor);
}

static struct dtm_model_entry*
__locate_model(struct dtm_vendor_bag* bag, struct hstr* model)
{
    return locate_entry(struct dtm_model_entry, bag->models, *model, model);
}

static int split_by(const char* compat, char delim,
                        char** part1, int* part1_len, 
                        char** part2, int* part2_len)
{
    int pos = 0;
    size_t len = strlen(compat);

    while (pos < len && compat[pos++] != delim);
    
    *part1 = &compat[0];
    *part1_len = pos;

    if (pos == len) {
        return 1;
    }

    *part2 = &compat[pos];
    *part2_len = len - pos;

    return 2;
}

bool
dtm_register_entry(struct device_def* def, 
                    const char* vendor, const char* model)
{
    struct hstr h_vendor, h_model;

    struct dtm_vendor_bag* vendor_bag;
    struct dtm_model_entry* model_ent;

    h_model  = HSTR(model, strlen(model));
    hstr_rehash(&h_model, HSTR_FULL_HASH);

    if (vendor) {
        h_vendor = HSTR(vendor, strlen(vendor));
        hstr_rehash(&h_vendor, HSTR_FULL_HASH);

        vendor_bag = __locate_vendor(vendor);
        if (!vendor_bag) {
            vendor_bag = valloc(sizeof(*vendor_bag));
            vendor_bag->vendor = h_vendor;
            hashtable_init(vendor_bag->models);

            hashtable_hash_in(registry, 
                            &vendor_bag->peers, HSTR_HASH(h_vendor));
        }
    }
    else {
        vendor_bag = &generic_drvs;
    }

    model_ent = __locate_model(vendor_bag, &h_model);
    if (model_ent) {
        WARN("device: %s,%s already registered, ignored.", vendor, model);
        return false;
    }

    model_ent = valloc(sizeof(*model_ent));
    model_ent->model = h_model;
    model_ent->devdef = def;

    hashtable_hash_in(vendor_bag->models, 
                      &model_ent->peers, HSTR_HASH(h_model));

    return true;
}

static struct device_def*
__try_get_devdef(const char* compat)
{
    int parts;
    char *vendor, *model;
    size_t vendor_len, model_len;
    struct dtm_vendor_bag* vendor_bag;

    struct hstr h_vendor, h_model;

    parts = split_by(compat, ',',
                    &vendor, &vendor_len, &model, &model_len);
        
    if (parts == 1) {
        h_model = HSTR(vendor, vendor_len);
        vendor_bag = &generic_drvs;
    }
    else {
        h_vendor = HSTR(vendor, vendor_len);
        h_model  = HSTR(model, model_len);
        hstr_rehash(&h_vendor, HSTR_FULL_HASH);

        vendor_bag = __locate_vendor(&h_vendor);

        if (!vendor_bag) {
            return NULL;
        } 
    }

    hstr_rehash(&h_model, HSTR_FULL_HASH);

    return __locate_model(vendor_bag, &h_model);
}

struct device_meta*
dtm_try_create(struct dt_node* node)
{
    const char* compat;
    struct device_def* devdef;
    struct device *dev;
    struct device_meta *parent_dev;
    struct dt_node_base* base;

    base = &node->base;
    parent_dev = dev_meta((struct device*)dt_parent(node)->binded_dev);
    parent_dev = parent_dev ? : dt_category;

    if (!base->compat.size) {
        struct device_cat* devcat;
        devcat = device_addcat(parent_dev, base->name);
        base->binded_dev = devcat;

        return dev_meta(devcat);
    }

    dtprop_strlst_foreach(compat, &base->compat)
    {
        devdef = __try_get_devdef(compat);

        if (devdef) {
            goto found;
        }
    }

    return NULL;

found:
    dev = vzalloc(sizeof(*dev));
    
    dev = device_allocsys(parent_dev, NULL);
    dev->devtree_node = node;
    
    int err;
    if ((err = devdef->bind(devdef, dev))) {
        WARN("failed to bind devtree node %s, err=%d", base->name, err);
        device_remove(dev);
        return NULL;
    }

    register_device(dev, &devdef->class, base->name);

    return dev;
}

static void
dtm_init()
{
    generic_drvs = (struct dtm_vendor_bag) { };

    hashtable_init(registry);
    hashtable_init(generic_drvs.models);

    dt_category = device_addcat(NULL, "tree");
}
owloysius_fetch_init(dtm_init, on_sysconf);