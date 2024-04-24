#include <lunaix/bcache.h>
#include <lunaix/mm/valloc.h>

static struct lru_zone* bcache_global_lru;

#define __lock(bc)  spin_lock(&((bc)->lock))
#define __unlock(bc)  spin_unlock(&((bc)->lock))

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

    __lock(cache);

    if (!spin_try_lock(&bnode->lock)) {
        __unlock(cache);
        return false;
    }
    
    __evict_internal_locked(bnode);
    btrie_remove(&cache->root, bnode->tag);
    llist_delete(&bnode->objs);

    spinlock_destory(&bnode->lock);
    vfree(bnode);

    return true;
}

bcache_zone_t
bcache_create_zone(char* name)
{
    return lru_new_zone(name, __try_evict_bcache);
}

void
bcache_init_zone(struct bcache* cache, struct lru_zone* lru, unsigned int log_ways, 
                   int cap, size_t blk_size, struct bcache_ops* ops)
{
    *cache = (struct bcache) {
        .lru = lru,
        .ops = *ops,
        .blksz = blk_size
    };

    btrie_init(&cache->root, log_ways);
    llist_init_head(&cache->objs);
    spinlock_init(&cache->lock);
}

bool
bcache_put(struct bcache* cache, unsigned long tag, void* block)
{
    struct bcache_node* node;

    __lock(cache);

    node = (struct bcache_node*)btrie_get(&cache->root, tag);

    if (node != NULL) {
        if (!__try_evict_bcache_locked(&node->lru_node)) {
            return false;
        }

        // Now the node is ready to be reused.
    }
    else {
        node = valloc(sizeof(*node));
        spinlock_init(&node->lock);
        btrie_set(&cache->root, tag, node);
    }
    
    *node = (struct bcache_node) {
        .data = block,
        .holder = cache,
        .tag = tag
    };

    lru_use_one(cache->lru, &node->lru_node);
    llist_append(&cache->objs, &node->objs);

    __unlock(cache);

    return true;
}

bool
bcache_tryhit_and_lock(struct bcache* cache, unsigned long tag, bcobj_t* result)
{
    struct bcache_node* node;

    __lock(cache);

    node = (struct bcache_node*)btrie_get(&cache->root, tag);
    if (!node) {
        return false;
    }

    __lock(node);

    lru_remove(cache->lru, &node->lru_node);
    *result = (bcobj_t)node;

    __unlock(cache);
    
    return true;
}

void
bcache_unlock(bcobj_t obj)
{
    struct bcache_node* node = (struct bcache_node*) obj;

    /*
        We must not lock the cache here.
        Deadlock could happened with `bcache_tryhit_and_lock`
        and `bcache_evict`
    */

    lru_use_one(node->holder->lru, &node->lru_node);
    __unlock(node);
}

void
bcache_evict(struct bcache* cache, unsigned long tag)
{
    struct bcache_node* node;

    __lock(cache);

    node = (struct bcache_node*)btrie_get(&cache->root, tag);
    
    if (!node) {
        return;
    }

    __lock(node);

    __evict_internal_locked(&node->lru_node);

    btrie_remove(&cache->root, tag);
    lru_remove(cache->lru, &node->lru_node);
    llist_delete(&node->objs);

    spinlock_destory(&node->lock);

    vfree(node);

    __unlock(cache);
}

static void
bcache_flush_locked(struct bcache* cache)
{
    struct bcache_node *pos, *n;
    llist_for_each(pos, n, &cache->objs, objs) {
        __lock(pos);

        __evict_internal_locked(&pos->lru_node);
        btrie_remove(&cache->root, pos->tag);
        lru_remove(cache->lru, &pos->lru_node);
        llist_delete(&pos->objs);

        __unlock(pos);
    }
}

void
bcache_flush(struct bcache* cache)
{
    __lock(cache);
    
    bcache_flush_locked(cache);

    __unlock(cache);
}

void
bcache_free(struct bcache* cache)
{
    __lock(cache);
    
    bcache_flush_locked(cache);
    btrie_release(cache);

    __unlock(cache);

    vfree(cache);
}

void
bcache_zone_free(bcache_zone_t zone)
{
    lru_free_zone(zone);
}