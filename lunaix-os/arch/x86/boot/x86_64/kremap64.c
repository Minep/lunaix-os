#define __BOOT_CODE__

#include <asm/boot_stage.h>
#include <asm/mm_defs.h>
#include <asm-generic/init_pagetable.h>

#define RSVD_PAGES 32

struct kernel_map 
{
    struct {
        pte_t _lft[_PAGE_LEVEL_SIZE];
    } krsvd[RSVD_PAGES];
} align(8);

static struct kernel_map kpt __section(".kpg");
export_symbol(debug, boot, kpt);

static ptr_t boot_text
do_remap()
{
    struct pt_alloc alloc;
    struct ptw_state ptw;
    pte_t pte;

    init_pt_alloc(&alloc, to_kphysical(&kpt), sizeof(kpt));
    init_ptw_state(&ptw, &alloc, kpt_alloc_table(&alloc));

    pte = pte_mkhuge(mkpte_prot(KERNEL_PGTAB));
    kpt_set_ptes(&ptw, 0, pte, L1T_SIZE, 4);

    kpt_mktable_at(&ptw, VMAP, L0T_SIZE);

#if LnT_ENABLED(3)
    size_t gran  = L3T_SIZE;
#else
    size_t gran  = L2T_SIZE;
#endif

    kpt_mktable_at(&ptw, KMAP, gran);
    kpt_mktable_at(&ptw, PMAP, gran);

    kpt_migrate_highmem(&ptw);

    pte = mkpte(__ptr(ptw.root), KERNEL_PGTAB);
    kpt_set_ptes(&ptw, VMS_SELF, pte, L0T_SIZE, 1);

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