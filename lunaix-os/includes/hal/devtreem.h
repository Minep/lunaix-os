#ifndef __LUNAIX_DEVTREE_TOP_H
#define __LUNAIX_DEVTREE_TOP_H

struct device_def;
struct device;

#ifdef CONFIG_USE_DEVICETREE

#include "devtree.h"
#include <lunaix/ds/hashtable.h>
#include <lunaix/ds/list.h>
#include <klibc/hash.h>

typedef struct dtn* devtree_link_t;

#define dt_node_morpher     morphable_attrs(dtn, mobj)

struct dtm_driver_info
{
    struct list_node node;
    const char* pattern;
};

struct dtm_driver_record
{
    struct hlist_node node;
    struct list_head infos;
    struct device_def* def;
};

void
dtm_register_entry(struct device_def* def, const char* pattern);

#else

#include <lunaix/types.h>

typedef void* devtree_link_t;

static inline void
dtm_register_entry(struct device_def* def, const char* pattern)
{
    return;
}

#endif

#endif /* __LUNAIX_DEVTREE_TOP_H */
