#include <klibc/string.h>
#include <lunaix/boot_generic.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <lunaix/kcmd.h>
#include <sys/mm/mm_defs.h>

/**
 * @brief Reserve memory for kernel bootstrapping initialization
 *
 * @param bhctx
 */
void
boot_begin(struct boot_handoff* bhctx)
{
    bhctx->prepare(bhctx);
    
    // Identity-map the first 3GiB address spaces
    pte_t* ptep  = mkl0tep(mkptep_va(VMS_SELF, 0));
    pte_t pte    = mkpte(0, KERNEL_DATA);
    size_t count = page_count(KERNEL_RESIDENT, L0T_SIZE);
    vmm_set_ptes_contig(ptep, pte, L0T_SIZE, count);

    struct boot_mmapent *mmap = bhctx->mem.mmap, *mmapent;
    for (size_t i = 0; i < bhctx->mem.mmap_len; i++) {
        mmapent = &mmap[i];
        size_t size_pg = leaf_count(mmapent->size);
        pfn_t start_pfn = pfn(mmapent->start);

        if (mmapent->type == BOOT_MMAP_FREE) {
            pmm_mark_chunk_free(start_pfn, size_pg);
            continue;
        }
    }

    /* Reserve region for all loaded modules */
    for (size_t i = 0; i < bhctx->mods.mods_num; i++) {
        struct boot_modent* mod = &bhctx->mods.entries[i];
        unsigned int counts = leaf_count(mod->end - mod->start);

        pmm_mark_chunk_occupied(pfn(mod->start), counts, PP_FGLOCKED);
    }
}

extern u8_t __kboot_end; /* link/linker.ld */

/**
 * @brief Release memory for kernel bootstrapping initialization
 *
 * @param bhctx
 */
void
boot_end(struct boot_handoff* bhctx)
{
    struct boot_mmapent *mmap = bhctx->mem.mmap, *mmapent;
    for (size_t i = 0; i < bhctx->mem.mmap_len; i++) {
        mmapent = &mmap[i];
        size_t size_pg = PN(ROUNDUP(mmapent->size, PG_SIZE));

        if (mmapent->type == BOOT_MMAP_RCLM) {
            pmm_mark_chunk_free(PN(mmapent->start), size_pg);
        }

        if (mmapent->type == BOOT_MMAP_FREE) {
            continue;
        }
    }

    bhctx->release(bhctx);
}

/**
 * @brief Clean up the boot stage code and data
 *
 */
void
boot_cleanup()
{
    pte_t* ptep  = mkl0tep(mkptep_va(VMS_SELF, 0));
    size_t count = page_count(KERNEL_RESIDENT, L0T_SIZE);
    vmm_unset_ptes(ptep, count);
}

void
boot_parse_cmdline(struct boot_handoff* bhctx) {
    if (!bhctx->kexec.len) {
        return;
    }

    kcmd_parse_cmdline(bhctx->kexec.cmdline);
}