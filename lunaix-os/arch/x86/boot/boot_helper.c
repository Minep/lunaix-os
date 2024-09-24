#include <lunaix/boot_generic.h>
#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/spike.h>
#include <lunaix/sections.h>

#include <asm/mm_defs.h>
#include <sys/boot/bstage.h>
#include <sys-generic/bootmem.h>

#ifdef CONFIG_ARCH_X86_64

void
boot_begin_arch_reserve(struct boot_handoff* bhctx)
{
    return;
}


void
boot_clean_arch_reserve(struct boot_handoff* bhctx)
{
    pfn_t start;

    start = leaf_count(__ptr(__kboot_start));
    pmm_unhold_range(start, leaf_count(__ptr(__kboot_end)) - start);
}

#else

#include <lunaix/mm/vmm.h>

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

extern void
mb_parse(struct boot_handoff* bhctx);

struct boot_handoff*
prepare_boot_handover()
{
    struct boot_handoff* handoff;

    handoff = bootmem_alloc(sizeof(*handoff));

    mb_parse(handoff);

    return handoff;
}