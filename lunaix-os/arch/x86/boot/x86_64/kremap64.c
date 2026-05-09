#define __BOOT_CODE__

#include <asm/boot_stage.h>
#include <asm/mm_defs.h>
#include <asm-generic/init_pagetable.h>

#define RSVD_PAGES 32

struct kernel_map 
{
    // reserved page table for kernel remapping
    struct {
        pte_t _lft[1 << _LFT_LEVEL_WIDTH];
    } krsvd[RSVD_PAGES];
} align(8);

static struct kernel_map kpt __section(".kpg");
export_symbol(debug, boot, kpt);

static ptr_t boot_text
do_remap()
{
    struct pt_alloc alloc;
    struct ptw_state ptw;
    int nr_tables;
    pte_t pte, *ptep;

    init_pt_alloc(&alloc, to_kphysical(&kpt), sizeof(kpt));
    init_ptw_state(&ptw, &alloc, kpt_alloc_table(&alloc));

    pte = pte_mkhuge(mkpte_prot(KERNEL_PGTAB));
    kpt_set_ptes(&ptw, 0, pte, L1T_SIZE, 4);
    
    // prealloc immediate tables for kernel area.

    nr_tables = (L0T_LENGTH - level_index(KERNEL_AREA_BEGIN, L0T));
    ptep = (pte_t*)ptw.root;
    ptep += L0T_LENGTH - nr_tables;

    while (nr_tables-- > 0) {
        pte = mkpte_root(kpt_alloc_table(&alloc), KERNEL_PGTAB);
        set_pte(ptep++, pte);
    }

    // build up the table hierarchies for important areas.

#if LnT_ENABLED(3)
    size_t gran  = L3T_SIZE;
#else
    size_t gran  = L2T_SIZE;
#endif

    kpt_mktable_at(&ptw, mempart(VMEM_REMAP_ZONE), gran);
    kpt_mktable_at(&ptw, mempart(PHYPAGE_MAP), gran);

    kpt_migrate_highmem(&ptw);

    return __ptr(ptw.root);
}

ptr_t boot_text
remap_kernel()
{    
    asm volatile("movq %1, %%rdi\n"
                 "rep stosb\n" ::"c"(sizeof(kpt)),
                 "r"(to_kphysical(&kpt)),
                 "a"(0)
                 : "rdi", "memory");

    return do_remap();
}
