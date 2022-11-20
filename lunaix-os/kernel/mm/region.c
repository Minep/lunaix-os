#include <lunaix/mm/region.h>
#include <lunaix/mm/valloc.h>

struct mm_region*
region_add(struct llist_header* lead,
           unsigned long start,
           unsigned long end,
           unsigned int attr)
{
    struct mm_region* region = valloc(sizeof(struct mm_region));

    *region = (struct mm_region){ .attr = attr, .end = end, .start = start };

    if (llist_empty(lead)) {
        llist_append(lead, &region->head);
        return region;
    }

    struct mm_region *pos, *n;
    llist_for_each(pos, n, lead, head)
    {
        if (start >= pos->end && end <= n->start) {
            break;
        }
    }

    llist_insert_after(&pos->head, &region->head);
    return region;
}

void
region_release_all(struct llist_header* lead)
{
    struct mm_region *pos, *n;

    llist_for_each(pos, n, lead, head)
    {
        vfree(pos);
    }
}

void
region_copy(struct llist_header* src, struct llist_header* dest)
{
    if (!src) {
        return;
    }

    struct mm_region *pos, *n;

    llist_for_each(pos, n, src, head)
    {
        region_add(dest, pos->start, pos->end, pos->attr);
    }
}

struct mm_region*
region_get(struct llist_header* lead, unsigned long vaddr)
{
    if (llist_empty(lead)) {
        return NULL;
    }

    struct mm_region *pos, *n;

    llist_for_each(pos, n, lead, head)
    {
        if (pos->start <= vaddr && vaddr < pos->end) {
            return pos;
        }
    }
    return NULL;
}
