#include <lunaix/ds/lru.h>
#include <lunaix/mm/valloc.h>

struct llist_header zone_lead = { .next = &zone_lead, .prev = &zone_lead };

struct lru_zone*
lru_new_zone(evict_cb try_evict_cb)
{
    struct lru_zone* zone = vzalloc(sizeof(struct lru_zone));
    if (!zone) {
        return NULL;
    }

    zone->try_evict = try_evict_cb;

    llist_init_head(&zone->lead_node);
    llist_append(&zone_lead, &zone->zones);

    return zone;
}

void
lru_use_one(struct lru_zone* zone, struct lru_node* node)
{
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
        llist_append(&zone->lead_node, elem);
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
lru_remove(struct lru_zone* zone, struct lru_node* node)
{
    if (node->lru_nodes.next && node->lru_nodes.prev) {
        llist_delete(&node->lru_nodes);
    }
    zone->objects--;
}