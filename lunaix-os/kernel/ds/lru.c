#include <lunaix/ds/lru.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <klibc/string.h>

struct llist_header zone_lead = { .next = &zone_lead, .prev = &zone_lead };

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

    return zone;
}

void
lru_free_zone(struct lru_zone* zone)
{
    lru_evict_all(zone);

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
}

void
lru_use_one(struct lru_zone* zone, struct lru_node* node)
{
    assert(!zone->delayed_free);

    if (node->lru_nodes.next && node->lru_nodes.prev) {
        llist_delete(&node->lru_nodes);
    }

    llist_prepend(&zone->lead_node, &node->lru_nodes);
    zone->objects++;
}

static void
__do_evict(struct lru_zone* zone, struct llist_header* elem)
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
}

void
lru_evict_one(struct lru_zone* zone)
{
    struct llist_header* tail = zone->lead_node.prev;
    if (tail == &zone->lead_node) {
        return;
    }

    __do_evict(zone, tail);
}

void
lru_evict_half(struct lru_zone* zone)
{
    int target = (int)(zone->objects / 2);
    struct llist_header* tail = zone->lead_node.prev;
    while (tail != &zone->lead_node && target > 0) {
        __do_evict(zone, tail);
        tail = tail->prev;
        target--;
    }
}

void
lru_evict_all(struct lru_zone* zone)
{
    struct llist_header* tail = zone->lead_node.prev;
    while (tail != &zone->lead_node) {
        __do_evict(zone, tail);
        tail = tail->prev;
    }
}

void
lru_remove(struct lru_zone* zone, struct lru_node* node)
{
    if (node->lru_nodes.next && node->lru_nodes.prev) {
        llist_delete(&node->lru_nodes);
    }
    zone->objects--;
}