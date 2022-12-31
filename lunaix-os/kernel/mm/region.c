#include <lunaix/mm/page.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <klibc/string.h>

struct mm_region*
region_create(ptr_t start, ptr_t end, u32_t attr)
{
    assert_msg(PG_ALIGNED(start), "not page aligned");
    assert_msg(PG_ALIGNED(end), "not page aligned");
    struct mm_region* region = valloc(sizeof(struct mm_region));
    *region =
      (struct mm_region){ .attr = attr, .start = start, .end = end - 1 };
    return region;
}

struct mm_region*
region_create_range(ptr_t start, size_t length, u32_t attr)
{
    assert_msg(PG_ALIGNED(start), "not page aligned");
    assert_msg(PG_ALIGNED(length), "not page aligned");
    struct mm_region* region = valloc(sizeof(struct mm_region));
    *region = (struct mm_region){ .attr = attr,
                                  .start = start,
                                  .end = start + length - 1 };
    return region;
}

void
region_add(vm_regions_t* lead, struct mm_region* vmregion)
{
    if (llist_empty(lead)) {
        llist_append(lead, &vmregion->head);
        return;
    }

    ptr_t cur_end = 0;
    struct mm_region *pos = (struct mm_region*)lead,
                     *n = list_entry(lead->next, struct mm_region, head);
    do {
        if (vmregion->start >= cur_end && vmregion->end <= n->start) {
            break;
        }
        cur_end = n->end;
        pos = n;
        n = list_entry(n->head.next, struct mm_region, head);
    } while ((ptr_t)&pos->head != (ptr_t)lead);

    // XXX caution. require mm_region::head to be the lead of struct
    llist_insert_after(&pos->head, &vmregion->head);
}

void
region_release(pid_t pid, struct mm_region* region)
{
    if (region->destruct_region) {
        region->destruct_region(region);
    }

    if (region->mfile) {
        vfs_pclose(region->mfile, pid);
    }

    vfree(region);
}

void
region_release_all(pid_t pid, vm_regions_t* lead)
{
    struct mm_region *pos, *n;

    llist_for_each(pos, n, lead, head)
    {
        region_release(pid, pos);
    }
}

void
region_copy(vm_regions_t* src, vm_regions_t* dest)
{
    if (!src) {
        return;
    }

    struct mm_region *pos, *n, *dup;

    llist_for_each(pos, n, src, head)
    {
        dup = valloc(sizeof(struct mm_region));
        memcpy(dup, pos, sizeof(*pos));
        region_add(dest, dup);
    }
}

struct mm_region*
region_get(vm_regions_t* lead, unsigned long vaddr)
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
