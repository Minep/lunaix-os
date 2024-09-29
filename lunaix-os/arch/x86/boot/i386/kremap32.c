#define __BOOT_CODE__

#include <asm/boot_stage.h>
#include <asm/mm_defs.h>
#include <asm-generic/init_pagetable.h>

// define the initial page table layout
struct kernel_map 
{
    struct {
        pte_t _lft[_PAGE_LEVEL_SIZE];
    } kernel_lfts[16];
} align(4);

static struct kernel_map kernel_pt __section(".kpg");
export_symbol(debug, boot, kernel_pt);

static ptr_t boot_text
do_remap()
{
    struct pt_alloc alloc;
    struct ptw_state ptw;
    pte_t pte;

    init_pt_alloc(&alloc, to_kphysical(&kernel_pt), sizeof(kernel_pt));
    init_ptw_state(&ptw, &alloc, kpt_alloc_table(&alloc));

    pte = pte_mkhuge(mkpte_prot(KERNEL_DATA));
    kpt_set_ptes(&ptw, 0, pte, L0T_SIZE, 1);

    kpt_mktable_at(&ptw, KMAP, L0T_SIZE);
    kpt_mktable_at(&ptw, VMAP, L0T_SIZE);

    kpt_migrate_highmem(&ptw);

    pte = mkpte(__ptr(ptw.root), KERNEL_PGTAB);
    kpt_set_ptes(&ptw, VMS_SELF, pte, L0T_SIZE, 1);

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