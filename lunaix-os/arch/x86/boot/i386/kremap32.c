#define __BOOT_CODE__

#include <asm/boot_stage.h>
#include <asm/mm_defs.h>
#include <asm-generic/init_pagetable.h>

// define the initial page table layout
struct kernel_map 
{
    struct {
        pte_t _lft[1 << _LFT_LEVEL_WIDTH];
    } kernel_lfts[16];
} align(4);

static struct kernel_map kernel_pt __section(".kpg");
export_symbol(debug, boot, kernel_pt);

static ptr_t boot_text
do_remap()
{
    struct pt_alloc alloc;
    struct ptw_state ptw;
    int nr_tables;
    pte_t pte, *ptep;

    init_pt_alloc(&alloc, to_kphysical(&kernel_pt), sizeof(kernel_pt));
    init_ptw_state(&ptw, &alloc, kpt_alloc_table(&alloc));

    pte = pte_mkhuge(mkpte_prot(KERNEL_DATA));
    kpt_set_ptes(&ptw, 0, pte, L0T_SIZE, 1);

    nr_tables = (L0T_LENGTH - level_index(KERNEL_AREA_BEGIN, L0T));
    ptep = (pte_t*)ptw.root;
    ptep += L0T_LENGTH - nr_tables;

    while (nr_tables-- > 0) {
        pte = mkpte_root(kpt_alloc_table(&alloc), KERNEL_PGTAB);
        set_pte(ptep++, pte);
    }

    kpt_mktable_at(&ptw, mempart(VMEM_REMAP_ZONE), L0T_SIZE);
    kpt_mktable_at(&ptw, mempart(PHYPAGE_MAP), L0T_SIZE);

    kpt_migrate_highmem(&ptw);

    return __ptr(ptw.root);
}

ptr_t boot_text
remap_kernel()
{
    ptr_t kmap_pa = to_kphysical(&kernel_pt);
    for (size_t i = 0; i < sizeof(kernel_pt); i++) {
        ((u8_t*)kmap_pa)[i] = 0;
    }

    return do_remap();
}
