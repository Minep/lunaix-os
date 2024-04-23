#include <lunaix/bcache.h>
#include <lunaix/mm/valloc.h>

static struct lru_zone* bcache_global_lru;

static bool 
__try_evict_bcache_locked(struct bcache_node* node)
{
    struct bcache* cache;

    cache = node->holder;
    cache->ops.sync_cached(cache, node->tag, node->data);

    return cache->ops.release_on_evict(node->data);
}

static int 
__try_evict_bcache(struct lru_node* node)
{
    struct bcache_node* bnode;
    
    bnode = container_of(node, struct bcache_node, lru_node);

    if (!spin_try_lock(&bnode->lock)) {
        return 0;
    }
    
    if (__try_evict_bcache_locked(bnode)) {
        return true;
    }

    spin_unlock(&bnode->lock);
    return false;
}

void
bcache_init(struct bcache* cache, char* name, unsigned int ways, 
            int cap, size_t blk_size, struct bcache_ops* ops)
{
    *cache = (struct bcache) {
        .lru = lru_new_zone(name, __try_evict_bcache),
        .ops = *ops,
        .blksz = blk_size
    };
}

bool
bcache_put(struct bcache* cache, unsigned long tag, void* block)
{
    struct bcache_node* node;

    node = (struct bcache_node*)btrie_get(&cache->root, tag);

    if (node != NULL) {
        if (!__try_evict_bcache_locked(&node->lru_node)) {
            return false;
        }

        // Now the node is ready to be reused.
    }
    else {
        node = valloc(sizeof(*node));
        btrie_set(&cache->root, tag, node);
    }
    
    *node = (struct bcache_node) {
        .data = block,
        .holder = cache,
        .tag = tag
    };

    lru_use_one(cache->lru, &node->lru_node);

    return true;
}

bool
bcache_tryhit_ref(struct bcache* cache, unsigned long tag, bcobj_t* result)
{
    struct bcache_node* node;

    node = (struct bcache_node*)btrie_get(&cache->root, tag);
    if (!node) {
        return false;
    }

    spin_lock(&node->lock);

    lru_use_one(cache->lru, &node->lru_node);
    *result = (bcobj_t)node;
    
    return true;
}

void
bcache_tryhit_unref(struct bcache* cache, bcobj_t obj)
{
    struct bcache_node* node = (struct bcache_node*) obj;

    spin_unlock(&node->lock);
}

void
bcache_evict(struct bcache* cache, unsigned long tag)
{
    struct bcache_node* node;

    node = (struct bcache_node*)btrie_get(&cache->root, tag);
    
    if (!node) {
        return;
    }

    spin_lock(&node->lock);

    if (!__try_evict_bcache_locked(&node->lru_node)) {
        return;
    }

    btrie_remove(&cache->root, tag);
    lru_remove(cache->lru, &node->lru_node);

    vfree(node);
}

void
bcache_flush(struct bcache* cache)
{
    lru_evict_all(cache->lru);
}