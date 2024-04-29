#include <lunaix/ds/lru.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/fs/twifs.h>

#include <klibc/string.h>

static struct llist_header zone_lead = { .next = &zone_lead, .prev = &zone_lead };

DEFINE_SPINLOCK_OPS(struct lru_zone*, lock);


static void
__do_evict_lockless(struct lru_zone* zone, struct llist_header* elem)
{
    llist_delete(elem);
    if (!zone->try_evict(container_of(elem, struct lru_node, lru_nodes))) {
        // if the node is unable to evict, raise it's rank by one, so
        // others can have chance in the next round
        struct llist_header* new_tail = zone->lead_node.prev;
        llist_prepend(new_tail, elem);
    } else {
        zone->objects--;
    }

    zone->evict_stats.n_single++;
}

static void
__lru_evict_all_lockness(struct lru_zone* zone)
{
    struct llist_header* tail = zone->lead_node.prev;
    while (tail != &zone->lead_node) {
        __do_evict_lockless(zone, tail);
        tail = tail->prev;
    }
}

struct lru_zone*
lru_new_zone(const char* name, evict_cb try_evict_cb)
{
    struct lru_zone* zone = vzalloc(sizeof(struct lru_zone));
    if (!zone) {
        return NULL;
    }

    zone->try_evict = try_evict_cb;

    strncpy(zone->name, name, sizeof(zone->name) - 1);
    llist_init_head(&zone->lead_node);
    llist_append(&zone_lead, &zone->zones);
    spinlock_init(&zone->lock);

    return zone;
}

void
lru_free_zone(struct lru_zone* zone)
{
    lock(zone);

    __lru_evict_all_lockness(zone);

    if (llist_empty(&zone->lead_node)) {
        llist_delete(&zone->zones);
        vfree(zone);
        return;
    }

    /*
        We are unable to free it at this moment,
        (probably due to tricky things happened
        to some cached object). Thus mark it and
        let the daemon try to free it asynchronously
    */
    zone->delayed_free = true;
    zone->attempts++;

    unlock(zone);
}

void
lru_use_one(struct lru_zone* zone, struct lru_node* node)
{
    lock(zone);

    assert(!zone->delayed_free);

    if (node->lru_nodes.next && node->lru_nodes.prev) {
        llist_delete(&node->lru_nodes);
    }
    else {
        zone->objects++;
    }

    llist_prepend(&zone->lead_node, &node->lru_nodes);
    zone->hotness++;

    unlock(zone);
}

void
lru_evict_one(struct lru_zone* zone)
{
    lock(zone);

    struct llist_header* tail = zone->lead_node.prev;
    if (tail == &zone->lead_node) {
        return;
    }

    __do_evict_lockless(zone, tail);

    unlock(zone);
}

void
lru_evict_half(struct lru_zone* zone)
{
    lock(zone);

    int target = (int)(zone->objects / 2);
    struct llist_header* tail = zone->lead_node.prev;
    while (tail != &zone->lead_node && target > 0) {
        __do_evict_lockless(zone, tail);
        tail = tail->prev;
        target--;
    }

    zone->evict_stats.n_half++;

    unlock(zone);
}

void
lru_evict_all(struct lru_zone* zone)
{
    lock(zone);
    
    __lru_evict_all_lockness(zone);

    zone->evict_stats.n_full++;

    unlock(zone);
}

void
lru_remove(struct lru_zone* zone, struct lru_node* node)
{
    lock(zone);

    if (node->lru_nodes.next && node->lru_nodes.prev) {
        llist_delete(&node->lru_nodes);
    }
    zone->objects--;

    unlock(zone);
}

static void
read_lrulist_entry(struct twimap* map)
{
    struct lru_zone* zone;

    zone = twimap_index(map, struct lru_zone*);
    twimap_printf(map, "%s, %d, %d, %d, %d, %d, ", 
                        zone->name, 
                        zone->objects,
                        zone->hotness,
                        zone->evict_stats.n_single,
                        zone->evict_stats.n_half,
                        zone->evict_stats.n_full);

    if (zone->delayed_free) {
        twimap_printf(map, "freeing %d attempts\n", zone->attempts);
    }
    else {
        twimap_printf(map, "active\n");
    }
}

static void
read_lrulist_reset(struct twimap* map)
{
    map->index = container_of(&zone_lead, struct lru_zone, zones);
    twimap_printf(map, "name, n_objs, hot, n_evt, n_half, n_full, status\n");
}

static int
read_lrulist_next(struct twimap* map)
{
    struct lru_zone* zone;
    struct llist_header* next;

    zone = twimap_index(map, struct lru_zone*);
    next = zone->zones.next;
    if (next == &zone_lead) {
        return false;
    }
    
    map->index = container_of(next, struct lru_zone, zones);
    return true;
}

static void
lru_pool_twimappable()
{
    struct twimap* map;

    map = twifs_mapping(NULL, NULL, "lru_pool");
    map->read = read_lrulist_entry;
    map->reset = read_lrulist_reset;
    map->go_next = read_lrulist_next;
}
EXPORT_TWIFS_PLUGIN(__lru_twimap, lru_pool_twimappable);