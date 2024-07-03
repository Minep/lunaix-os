#include <lunaix/boot_generic.h>
#include <lunaix/mm/pagetable.h>

#include "sys/mm/mm_defs.h"

#ifdef CONFIG_ARCH_X86_64

void
boot_begin_arch_reserve(struct boot_handoff* bhctx)
{
    return;
}


void
boot_clean_arch_reserve(struct boot_handoff* bhctx)
{
    return;
}

#else

void
boot_begin_arch_reserve(struct boot_handoff* bhctx)
{
    // Identity-map the first 3GiB address spaces
    pte_t* ptep  = mkl0tep(mkptep_va(VMS_SELF, 0));
    pte_t pte    = mkpte_prot(KERNEL_DATA);
    size_t count = page_count(KERNEL_RESIDENT, L0T_SIZE);

    vmm_set_ptes_contig(ptep, pte_mkhuge(pte), L0T_SIZE, count);
}


void
boot_clean_arch_reserve(struct boot_handoff* bhctx)
{
    pte_t* ptep  = mkl0tep(mkptep_va(VMS_SELF, 0));
    size_t count = page_count(KERNEL_RESIDENT, L0T_SIZE);
    vmm_unset_ptes(ptep, count);
}


#endif