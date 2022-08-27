#include <lunaix/mm/region.h>
#include <lunaix/mm/valloc.h>

void
region_add(struct mm_region* regions,
           unsigned long start,
           unsigned long end,
           unsigned int attr)
{
    struct mm_region* region = valloc(sizeof(struct mm_region));

    *region = (struct mm_region){ .attr = attr, .end = end, .start = start };

    llist_append(&regions->head, &region->head);
}

void
region_release_all(struct mm_region* regions)
{
    struct mm_region *pos, *n;

    llist_for_each(pos, n, &regions->head, head)
    {
        vfree(pos);
    }
}

void
region_copy(struct mm_region* src, struct mm_region* dest)
{
    if (!src) {
        return;
    }

    struct mm_region *pos, *n;

    llist_for_each(pos, n, &src->head, head)
    {
        region_add(dest, pos->start, pos->end, pos->attr);
    }
}

struct mm_region*
region_get(struct mm_region* regions, unsigned long vaddr)
{
    if (!regions) {
        return NULL;
    }

    struct mm_region *pos, *n;

    llist_for_each(pos, n, &regions->head, head)
    {
        if (pos->start <= vaddr && vaddr < pos->end) {
            return pos;
        }
    }
    return NULL;
}
