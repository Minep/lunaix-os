#include <lunaix/mm/region.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/process.h>

#include <sys/mm/mempart.h>

#include <klibc/string.h>

struct mm_region*
region_create(ptr_t start, ptr_t end, u32_t attr)
{
    assert_msg(!va_offset(start), "not page aligned");
    assert_msg(!va_offset(end), "not page aligned");
    struct mm_region* region = valloc(sizeof(struct mm_region));
    *region =
      (struct mm_region){ .attr = attr, .start = start, .end = end - 1 };
    return region;
}

struct mm_region*
region_create_range(ptr_t start, size_t length, u32_t attr)
{
    assert_msg(!va_offset(start), "not page aligned");
    assert_msg(!va_offset(length), "not page aligned");
    struct mm_region* region = valloc(sizeof(struct mm_region));
    *region = (struct mm_region){ .attr = attr,
                                  .start = start,
                                  .end = ROUNDUP(start + length, PAGE_SIZE) };
    return region;
}

struct mm_region*
region_dup(struct mm_region* origin)
{
    struct mm_region* region = valloc(sizeof(struct mm_region));
    *region = *origin;

    if (region->mfile) {
        vfs_ref_file(region->mfile);
    }

    llist_init_head(&region->head);
    return region;
}

void
region_add(vm_regions_t* lead, struct mm_region* vmregion)
{
    if (llist_empty(lead)) {
        llist_append(lead, &vmregion->head);
        return;
    }

    struct mm_region *pos, *n;
    ptr_t cur_end = 0;

    llist_for_each(pos, n, lead, head)
    {
        if (vmregion->start >= cur_end && vmregion->end <= pos->start) {
            break;
        }
        cur_end = pos->end;
    }

    // XXX caution. require mm_region::head to be the lead of struct
    llist_append(&pos->head, &vmregion->head);
}

void
region_release(struct mm_region* region)
{
    if (region->destruct_region) {
        region->destruct_region(region);
    }

    if (region->mfile) {
        struct proc_mm* mm = region->proc_vms;
        vfs_pclose(region->mfile, mm->proc->pid);
    }

    if (region->index) {
        *region->index = NULL;
    }

    vfree(region);
}

void
region_release_all(vm_regions_t* lead)
{
    struct mm_region *pos, *n;

    llist_for_each(pos, n, lead, head)
    {
        region_release(pos);
    }
}

void
region_copy_mm(struct proc_mm* src, struct proc_mm* dest)
{
    struct mm_region *pos, *n, *dup;

    llist_for_each(pos, n, &src->regions, head)
    {
        dup = valloc(sizeof(struct mm_region));
        memcpy(dup, pos, sizeof(*pos));

        dup->proc_vms = dest;

        if (dup->mfile) {
            vfs_ref_file(dup->mfile);
        }

        if (dup->region_copied) {
            dup->region_copied(dup);
        }

        llist_append(&dest->regions, &dup->head);
    }
}

struct mm_region*
region_get(vm_regions_t* lead, unsigned long vaddr)
{
    if (llist_empty(lead)) {
        return NULL;
    }

    struct mm_region *pos, *n;

    vaddr = page_aligned(vaddr);

    llist_for_each(pos, n, lead, head)
    {
        if (pos->start <= vaddr && vaddr < pos->end) {
            return pos;
        }
    }
    return NULL;
}
