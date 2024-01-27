#include <lunaix/mm/procvm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/region.h>
#include <lunaix/process.h>

#include <klibc/string.h>

struct proc_mm*
procvm_create(struct proc_info* proc) {
    struct proc_mm* mm = valloc(sizeof(struct proc_mm));

    assert(mm);

    mm->heap = 0;
    mm->stack = 0;
    mm->proc = proc;

    llist_init_head(&mm->regions);
    return mm;
}

void
procvm_dup(struct proc_info* proc) {
   struct proc_mm* mm = vmspace(proc);
   struct proc_mm* mm_current = vmspace(__current);
   
   mm->heap = mm_current->heap;
   mm->stack = mm_current->stack;
   
   region_copy_mm(mm_current, mm);
}

void
procvm_cleanup(ptr_t vm_mnt, struct proc_info* proc) {
    struct mm_region *pos, *n;
    llist_for_each(pos, n, vmregions(proc), head)
    {
        mem_sync_pages(vm_mnt, pos, pos->start, pos->end - pos->start, 0);
        region_release(pos);
    }

    vfree(proc->mm);
}