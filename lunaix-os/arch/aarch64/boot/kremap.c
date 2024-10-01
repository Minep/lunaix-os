#include <lunaix/sections.h>
#include <asm-generic/init_pagetable.h>
#include <asm/boot_stage.h>

#include "init.h"

static pte_t kpt[LEVEL_SIZE][32];

ptr_t
kremap()
{
    struct pt_alloc alloc;
    struct ptw_state ptw;
    pte_t pte;
    unsigned long nr;

    init_pt_alloc(&alloc, to_kphysical(&kpt), sizeof(kpt));
    init_ptw_state(&ptw, &alloc, kpt_alloc_table(&alloc));

    pte = mkpte(boot_start, KERNEL_DATA);
    pte = pte_mkexec(pte);
    nr  = leaf_count(boot_end - boot_start);
    kpt_set_ptes(&ptw, boot_start, pte, LFT_SIZE, nr);

    kpt_mktable_at(&ptw, VMAP, L0T_SIZE);
    kpt_mktable_at(&ptw, PMAP, L2T_SIZE);
    kpt_mktable_at(&ptw, KMAP, LFT_SIZE);

    kpt_migrate_highmem(&ptw);

    pte = mkpte(__ptr(ptw.root), KERNEL_PGTAB);
    kpt_set_ptes(&ptw, VMS_SELF, pte, L0T_SIZE, 1);

    return __ptr(ptw.root);
}