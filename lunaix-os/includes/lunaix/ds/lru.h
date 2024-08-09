#ifndef __LUNAIX_LRU_H
#define __LUNAIX_LRU_H

#include <lunaix/ds/llist.h>
#include <lunaix/ds/spinlock.h>
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
    char name[32];
    evict_cb try_evict;
    spinlock_t lock;

    unsigned int objects;
    unsigned int hotness;
    struct {
        unsigned int n_single;
        unsigned int n_half;
        unsigned int n_full;
    } evict_stats;

    union {
        struct {
            bool delayed_free:1;
            unsigned char attempts;
        };
        unsigned int flags;
    };
};

struct lru_zone*
lru_new_zone(const char* name, evict_cb try_evict_cb);

void
lru_use_one(struct lru_zone* zone, struct lru_node* node);

void
lru_evict_one(struct lru_zone* zone);

void
lru_remove(struct lru_zone* zone, struct lru_node* node);

void
lru_evict_half(struct lru_zone* zone);

void
lru_evict_all(struct lru_zone* zone);

void
lru_free_zone(struct lru_zone* zone);

#endif /* __LUNAIX_LRU_H */
