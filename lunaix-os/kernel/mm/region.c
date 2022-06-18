#include <lunaix/mm/kalloc.h>
#include <lunaix/mm/region.h>

void
region_add(struct mm_region** regions,
           unsigned long start,
           unsigned long end,
           unsigned int attr)
{
    struct mm_region* region = lxmalloc(sizeof(struct mm_region));

    *region = (struct mm_region){ .attr = attr, .end = end, .start = start };

    if (!*regions) {
        llist_init_head(&region->head);
        *regions = region;
    } else {
        llist_append(&(*regions)->head, &region->head);
    }
}

void
region_release_all(struct mm_region** regions)
{
    struct mm_region *pos, *n;

    llist_for_each(pos, n, &(*regions)->head, head)
    {
        lxfree(pos);
    }

    *regions = NULL;
}

void
region_copy(struct mm_region** src, struct mm_region** dest)
{
    if (!*src) {
        return;
    }

    struct mm_region *pos, *n;

    llist_init_head(*dest);
    llist_for_each(pos, n, &(*src)->head, head)
    {
        region_add(dest, pos->start, pos->end, pos->attr);
    }
}

struct mm_region*
region_get(struct mm_region** regions, unsigned long vaddr)
{
    if (!*regions) {
        return NULL;
    }

    struct mm_region *pos, *n;

    llist_for_each(pos, n, &(*regions)->head, head)
    {
        if (vaddr >= pos->start && vaddr < pos->end) {
            return pos;
        }
    }
    return NULL;
}
