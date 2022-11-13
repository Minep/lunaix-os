#ifndef __LUNAIX_LRU_H
#define __LUNAIX_LRU_H

#include <lunaix/ds/llist.h>
#include <lunaix/types.h>

struct lru_node
{
    struct llist_header lru_nodes;
};

typedef int (*evict_cb)(struct lru_node* lru_obj);

struct lru_zone
{
    struct llist_header lead_node;
    struct llist_header zones;
    u32_t objects;
    evict_cb try_evict;
};

struct lru_zone*
lru_new_zone(evict_cb try_evict_cb);

void
lru_use_one(struct lru_zone* zone, struct lru_node* node);

void
lru_evict_one(struct lru_zone* zone);

void
lru_remove(struct lru_zone* zone, struct lru_node* node);

void
lru_evict_half(struct lru_zone* zone);

#endif /* __LUNAIX_LRU_H */
