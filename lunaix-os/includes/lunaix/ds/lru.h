#ifndef __LUNAIX_LRU_H
#define __LUNAIX_LRU_H

#include <lunaix/ds/llist.h>

struct lru_zone
{
    struct llist_header lead_node;
    struct llist_header zones;
};

struct lru_node
{
    struct llist_header lru_nodes;
};

struct lru_zone*
lru_new_zone();

void
lru_use_one(struct lru_zone* zone, struct lru_node* node);

struct lru_node*
lru_evict_one(struct lru_zone* zone);

void
lru_remove(struct lru_node* node);

#endif /* __LUNAIX_LRU_H */
