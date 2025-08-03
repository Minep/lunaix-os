#include "lunaix/ds/spinlock.h"
#include <lunaix/bcache.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

static struct lru_zone* bcache_global_lru;

DEFINE_SPINLOCK_OPS(struct bcache*, bc, lock);

static void 
__evict_internal_locked(struct bcache_node* node)
{
    struct bcache* cache;

    cache = node->holder;
    cache->ops.sync_cached(cache, node->tag, node->data);

    cache->ops.release_on_evict(cache, node->data);
}

static int 
__try_evict_bcache(struct lru_node* node)
{
    struct bcache_node* bnode;
    struct bcache* cache;
    
    bnode = container_of(node, struct bcache_node, lru_node);
    cache = bnode->holder;
    
    lock_bc(cache);

    if (bnode->refs) {
        unlock_bc(cache);
        return false;
    }
    
    __evict_internal_locked(bnode);
    btrie_remove(&cache->root, bnode->tag);
    llist_delete(&bnode->objs);

    vfree(bnode);

    unlock_bc(cache);

    return true;
}

bcache_zone_t
bcache_create_zone(char* name)
{
    return lru_new_zone(name, __try_evict_bcache);
}

void
bcache_init_zone(struct bcache* cache, bcache_zone_t lru, unsigned int log_ways, 
                   int cap, unsigned int blk_size, struct bcache_ops* ops)
{
    // TODO handle cap
    
    *cache = (struct bcache) {
        .lru = lru,
        .ops = *ops,
        .blksz = blk_size
    };

    btrie_init(&cache->root, log_ways);
    llist_init_head(&cache->objs);
    spinlock_init(&cache->lock);
}

bcobj_t
bcache_put_and_ref(struct bcache* cache, unsigned long tag, void* block)
{
    struct bcache_node* node;

    lock_bc(cache);

    node = (struct bcache_node*)btrie_get(&cache->root, tag);

    if (node != NULL) {
        assert(!node->refs);
        __evict_internal_locked(node);
        // Now the node is ready to be reused.
    }
    else {
        node = vzalloc(sizeof(*node));
        btrie_set(&cache->root, tag, node);
    }
    
    *node = (struct bcache_node) {
        .data = block,
        .holder = cache,
        .tag = tag,
        .refs = 1
    };

    lru_use_one(cache->lru, &node->lru_node);
    llist_append(&cache->objs, &node->objs);

    unlock_bc(cache);

    return (bcobj_t)node;
}

bool
bcache_tryget(struct bcache* cache, unsigned long tag, bcobj_t* result)
{
    struct bcache_node* node;

    lock_bc(cache);

    node = (struct bcache_node*)btrie_get(&cache->root, tag);
    if (!node) {
        unlock_bc(cache);
        *result = NULL;

        return false;
    }

    node->refs++;

    *result = (bcobj_t)node;

    unlock_bc(cache);
    
    return true;
}

void
bcache_return(bcobj_t obj)
{
    struct bcache_node* node = (struct bcache_node*) obj;

    assert(node->refs);

    // non bisogno bloccare il cache, perche il lru ha la serratura propria.
    lru_use_one(node->holder->lru, &node->lru_node);
    node->refs--;
}

void
bcache_promote(bcobj_t obj)
{
    struct bcache_node* node;
    
    node = (struct bcache_node*) obj;
    assert(node->refs);
    lru_use_one(node->holder->lru, &node->lru_node);
}

void
bcache_evict(struct bcache* cache, unsigned long tag)
{
    struct bcache_node* node;

    lock_bc(cache);

    node = (struct bcache_node*)btrie_get(&cache->root, tag);
    
    if (!node || node->refs) {
        unlock_bc(cache);
        return;
    }

    __evict_internal_locked(node);

    btrie_remove(&cache->root, tag);
    lru_remove(cache->lru, &node->lru_node);
    llist_delete(&node->objs);

    vfree(node);

    unlock_bc(cache);
}

static void
bcache_flush_locked(struct bcache* cache)
{
    struct bcache_node *pos, *n;
    llist_for_each(pos, n, &cache->objs, objs) {
        __evict_internal_locked(pos);
        btrie_remove(&cache->root, pos->tag);
        lru_remove(cache->lru, &pos->lru_node);
        llist_delete(&pos->objs);
    }
}

void
bcache_flush(struct bcache* cache)
{
    lock_bc(cache);
    
    bcache_flush_locked(cache);

    unlock_bc(cache);
}

void
bcache_free(struct bcache* cache)
{
    lock_bc(cache);
    
    bcache_flush_locked(cache);
    btrie_release(&cache->root);

    unlock_bc(cache);

    vfree(cache);
}

void
bcache_zone_free(bcache_zone_t zone)
{
    lru_free_zone(zone);
}
