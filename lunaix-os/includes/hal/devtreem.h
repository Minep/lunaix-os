#ifndef __LUNAIX_DEVTREE_TOP_H
#define __LUNAIX_DEVTREE_TOP_H

struct device_def;
struct device;

#ifdef CONFIG_USE_DEVICETREE

#include "devtree.h"
#include <lunaix/ds/hashtable.h>
#include <klibc/hash.h>

typedef struct dt_node* devtree_link_t;

#define dt_node_morpher     morphable_attrs(dt_node, mobj)

struct dtm_vendor_bag
{
    struct hstr vendor;
    struct hlist_node peers; 
    DECLARE_HASHTABLE(models, 16);
};

struct dtm_model_entry
{
    struct hstr model;
    struct hlist_node peers;
    struct device_def* devdef;
};

bool
dtm_register_entry(struct device_def* def, 
                    const char* vendor, const char* model);

struct device*
dtm_try_create(struct device_def* def, struct dt_node* node);

#else

#include <lunaix/types.h>

typedef void* devtree_link_t;

static inline void
dtm_register_entry(struct device_def* def, 
                    const char* vendor, const char* model)
{
    return;
}

static inline struct device*
dtm_try_create(struct device_def* def, struct dt_node* node)
{
    return NULL;
}

#endif

#endif /* __LUNAIX_DEVTREE_TOP_H */
