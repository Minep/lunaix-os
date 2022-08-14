#include <lunaix/ds/lru.h>
#include <lunaix/mm/valloc.h>

struct llist_header zone_lead = { .next = &zone_lead, .prev = &zone_lead };

struct lru_zone*
lru_new_zone()
{
    struct lru_zone* zone = valloc(sizeof(struct lru_zone));
    if (!zone) {
        return NULL;
    }

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
}

struct lru_node*
lru_evict_one(struct lru_zone* zone)
{
    struct llist_header* tail = zone->lead_node.prev;
    if (tail == &zone->lead_node) {
        return;
    }

    llist_delete(tail);

    return container_of(tail, struct lru_node, lru_nodes);
}

void
lru_remove(struct lru_node* node)
{
    if (node->lru_nodes.next && node->lru_nodes.prev) {
        llist_delete(&node->lru_nodes);
    }
}