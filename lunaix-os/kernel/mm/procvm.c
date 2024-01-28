#include <lunaix/mm/procvm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>

#include <sys/mm/mempart.h>

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


static ptr_t
__dup_vmspace(ptr_t mount_point)
{
    ptr_t ptd_pp = pmm_alloc_page(PP_FGPERSIST);
    vmm_set_mapping(VMS_SELF, PG_MOUNT_1, ptd_pp, PG_PREM_RW, VMAP_NULL);

    x86_page_table* ptd = (x86_page_table*)PG_MOUNT_1;
    x86_page_table* pptd = (x86_page_table*)(mount_point | (0x3FF << 12));

    size_t kspace_l1inx = L1_INDEX(KERNEL_EXEC);

    for (size_t i = 0; i < PG_MAX_ENTRIES - 1; i++) {

        x86_pte_t ptde = pptd->entry[i];
        // 空或者是未在内存中的L1页表项直接照搬过去。
        // 内核地址空间直接共享过去。
        if (!ptde || i >= kspace_l1inx || !(ptde & PG_PRESENT)) {
            ptd->entry[i] = ptde;
            continue;
        }

        // 复制L2页表
        ptr_t pt_pp = pmm_alloc_page(PP_FGPERSIST);
        vmm_set_mapping(VMS_SELF, PG_MOUNT_2, pt_pp, PG_PREM_RW, VMAP_NULL);

        x86_page_table* ppt = (x86_page_table*)(mount_point | (i << 12));
        x86_page_table* pt = (x86_page_table*)PG_MOUNT_2;

        for (size_t j = 0; j < PG_MAX_ENTRIES; j++) {
            x86_pte_t pte = ppt->entry[j];
            pmm_ref_page(PG_ENTRY_ADDR(pte));
            pt->entry[j] = pte;
        }

        ptd->entry[i] = (ptr_t)pt_pp | PG_ENTRY_FLAGS(ptde);
    }

    ptd->entry[PG_MAX_ENTRIES - 1] = NEW_L1_ENTRY(T_SELF_REF_PERM, ptd_pp);

    return ptd_pp;
}

void
procvm_dup(struct proc_info* proc) {
   struct proc_mm* mm = vmspace(proc);
   struct proc_mm* mm_current = vmspace(__current);
   
   mm->heap = mm_current->heap;
   mm->stack = mm_current->stack;
   mm->vmroot = __dup_vmspace(VMS_SELF);
   
   region_copy_mm(mm_current, mm);
}


static void
__delete_vmspace(ptr_t vm_mnt)
{
    x86_page_table* pptd = (x86_page_table*)(vm_mnt | (0x3FF << 12));

    // only remove user address space
    for (size_t i = 0; i < L1_INDEX(KERNEL_EXEC); i++) {
        x86_pte_t ptde = pptd->entry[i];
        if (!ptde || !(ptde & PG_PRESENT)) {
            continue;
        }

        x86_page_table* ppt = (x86_page_table*)(vm_mnt | (i << 12));

        for (size_t j = 0; j < PG_MAX_ENTRIES; j++) {
            x86_pte_t pte = ppt->entry[j];
            // free the 4KB data page
            if ((pte & PG_PRESENT)) {
                pmm_free_page(PG_ENTRY_ADDR(pte));
            }
        }
        // free the L2 page table
        pmm_free_page(PG_ENTRY_ADDR(ptde));
    }
    // free the L1 directory
    pmm_free_page(PG_ENTRY_ADDR(pptd->entry[PG_MAX_ENTRIES - 1]));
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

    __delete_vmspace(vm_mnt);
}