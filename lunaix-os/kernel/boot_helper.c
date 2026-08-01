#include <lunaix/mm/pagetable.h>
#include <klibc/string.h>
#include <lunaix/boot_generic.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/vastm.h>
#include <lunaix/spike.h>
#include <lunaix/kcmd.h>
#include <lunaix/sections.h>
#include <asm/mm_defs.h>

/**
 * @brief Reserve memory for kernel bootstrapping initialization
 *
 * @param bhctx
 */
void
boot_begin(struct boot_handoff* bhctx)
{
    size_t pg_count;
    ptr_t kstart;

    bhctx->prepare(bhctx);

    boot_begin_arch_reserve(bhctx);
   
    // reserves all kernel occupied physical pages
    // note: we will reclaim the .bss.reclaimable upon existing
    // the boot context, which will essentially shrink the 
    // (kstart, kend)
    //
    kstart = to_kphysical(kernel_start);
    pg_count = to_kphysical(kernel_load_end) - kstart;
    pg_count = count_pages(pg_count) + 1;

    pmm_onhold_range(page_index(kstart), pg_count);

    size_t i;
    struct boot_mmapent* ent;
    for (i = 0; i < bhctx->mem.mmap_len; i++) {
        ent = &bhctx->mem.mmap[i];

        if (reserved_memregion(ent) || reclaimable_memregion(ent)) {
            unsigned int counts = count_pages(ent->size);
            pmm_onhold_range(page_index(ent->start), counts);
        }
    }

    /* Reserve region for all loaded modules */
    for (i = 0; i < bhctx->mods.mods_num; i++) {
        struct boot_modent* mod = &bhctx->mods.entries[i];
        unsigned int counts = count_pages(mod->end - mod->start);

        pmm_onhold_range(page_index(mod->start), counts);
    }
}

static void
__free_reclaimable()
{
    ptr_t start;
    pfn_t pgs;
    pte_t* ptep;

    start = reclaimable_start;
    pgs   = count_pages(reclaimable_end - start);
    ptep  = vastm_walk_ptep(vastm_current_root(), start, RES_LFT);

    pmm_unhold_range(page_index(to_kphysical(start)), pgs);
    fill_ptes(ptep, null_pte, pgs);
}

/**
 * @brief Release memory for kernel bootstrapping initialization
 *
 * @param bhctx
 */
void
boot_end(struct boot_handoff* bhctx)
{
    struct boot_mmapent* ent;
    for (size_t i = 0; i < bhctx->mem.mmap_len; i++) {
        ent = &bhctx->mem.mmap[i];

        if (!reclaimable_memregion(ent)) 
            continue;

        unsigned int counts = count_pages(ent->size);
        pmm_unhold_range(page_index(ent->start), counts);
    }

    bhctx->release(bhctx);

    boot_clean_arch_reserve(bhctx);

    // reclaim the .bss.reclaimable
    __free_reclaimable();
}

void
boot_parse_cmdline(struct boot_handoff* bhctx) {
    if (!bhctx->kexec.len) {
        return;
    }

    kcmd_parse_cmdline(bhctx->kexec.cmdline);
}
