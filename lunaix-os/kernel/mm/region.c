#include <lunaix/mm/region.h>
#include <lunaix/mm/kalloc.h>
#include <lunaix/process.h>

void region_add(struct proc_info* proc,unsigned long start, unsigned long end, unsigned int attr) {
    struct mm_region* region = lxmalloc(sizeof(struct mm_region));

    *region = (struct mm_region) {
        .attr = attr,
        .end = end,
        .start = start
    };

    if (!proc->mm.regions) {
        llist_init_head(&region->head);
        proc->mm.regions = region;
    }
    else {
        llist_append(&proc->mm.regions->head, &region->head);
    }
}

void region_release_all(struct proc_info* proc) {
    struct mm_region* head = proc->mm.regions;
    struct mm_region *pos, *n;

    llist_for_each(pos, n, &head->head, head) {
        lxfree(pos);
    }

    proc->mm.regions = NULL;
}

struct mm_region* region_get(struct proc_info* proc, unsigned long vaddr) {
    struct mm_region* head = proc->mm.regions;
    
    if (!head) {
        return NULL;
    }

    struct mm_region *pos, *n;

    llist_for_each(pos, n, &head->head, head) {
        if (vaddr >= pos->start && vaddr < pos->end) {
            return pos;
        }
    }
    return NULL;
}
