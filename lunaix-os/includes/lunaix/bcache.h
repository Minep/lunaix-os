#ifndef __LUNAIX_BCACHE_H
#define __LUNAIX_BCACHE_H

#include <lunaix/ds/btrie.h>
#include <lunaix/ds/lru.h>
#include <lunaix/ds/spin.h>
#include <lunaix/ds/llist.h>

/*
   Block cache. A cache built on top of
   sparse array (trie tree) allow caching
   any blocks that have spatial structure
   attach to them. With intention to unify
   all the existing caching construct, as
   well as potential future use case.
   
   NB. block is not necessarily
   equivalence to disk sector nor filesystem 
   logical block. block can be anything 
   discrete.

   NB2. not to be confused with page cahce
   (pcache), which is a special case of
   bcache.
*/

struct bcache_ops
{
    void (*release_on_evict)(struct bcache*, void* data);
    void (*sync_cached)(struct bcache*, unsigned long tag, void* data);
};

struct bcache
{
    struct btrie root;
    struct lru_zone* lru;
    struct bcache_ops ops;
    struct llist_header objs;
    struct spinlock lock;
    size_t blksz;
};  // block cache

struct bcache_node
{
    void* data;

    unsigned long tag;
    
    struct bcache* holder;
    struct spinlock lock;
    struct lru_node lru_node;
    struct llist_header objs;
};

typedef void * bcobj_t;
typedef struct lru_zone* bcache_zone_t;

static inline void*
bcached_data(bcobj_t obj)
{
    return ((struct bcache_node*)obj)->data;
}

/**
 * @brief Create a block cache
 * 
 * @param cache to be initialized
 * @param name name of this cache
 * @param log_ways ways-associative of this cache
 * @param cap capacity of this cache, -1 for 'infinity' cache
 * @param blk_size size of each cached object
 * @param ops block cache operation
 */
static inline void
bcache_init(struct bcache* cache, char* name, unsigned int log_ways, 
            int cap, size_t blk_size, struct bcache_ops* ops)
{
    bcache_init_zone(cache, bcache_create_zone(name), 
                     log_ways, cap, blk_size, ops);
}

/**
 * @brief Create a block cache with shared bcache zone
 * 
 * @param cache to be initialized
 * @param name name of this cache
 * @param log_ways ways-associative of this cache
 * @param cap capacity of this cache, -1 for 'infinity' cache
 * @param blk_size size of each cached object
 * @param ops block cache operation
 */
void
bcache_init_zone(struct bcache* cache, bcache_zone_t zone, unsigned int log_ways, 
                   int cap, size_t blk_size, struct bcache_ops* ops);


bcache_zone_t
bcache_create_zone(char* name);

bool
bcache_put(struct bcache* cache, unsigned long tag, void* block);

/**
 * @brief Try look for a hit and return the reference to block.
 * Now, this create a unmanaged pointer that could end up in
 * everywhere and unsafe to evict. One should called `bcache_tryhit_unref`
 * when the reference is no longer needed.
 * 
 * @param cache 
 * @param tag 
 * @param block_out 
 * @return true 
 * @return false 
 */
bool
bcache_tryhit_and_lock(struct bcache* cache, unsigned long tag, bcobj_t* result);

/**
 * @brief Unreference a cached block that is returned 
 * by `bcache_tryhit_ref`
 * 
 * @param cache 
 * @param tag 
 * @param block_out 
 * @return true 
 * @return false 
 */
void
bcache_unlock(bcobj_t obj);

void
bcache_evict(struct bcache* cache, unsigned long tag);

static inline void
bcache_evict_one(struct bcache* cache)
{
    lru_evict_one(cache->lru);
}

void
bcache_flush(struct bcache* cache);

void
bcache_free(struct bcache* cache);

void
bcache_zone_free(bcache_zone_t zone);

#endif /* __LUNAIX_BCACHE_H */
