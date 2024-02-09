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

    struct boot_mmapent *mmap = bhctx->mem.mmap, *mmapent;
    for (size_t i = 0; i < bhctx->mem.mmap_len; i++) {
        mmapent = &mmap[i];
        size_t size_pg = page_count(mmapent->size);
        pfn_t start_pfn = pfn(mmapent->start);

        if (mmapent->type == BOOT_MMAP_FREE) {
            pmm_mark_chunk_free(start_pfn, size_pg);
            continue;
        }

        if (mmapent->start >= KERNEL_RESIDENT) {
            continue;
        }

        pte_t* ptep = mkptep_va(VMS_SELF, mmapent->start);
        pte_t pte = mkpte(mmapent->start, KERNEL_DATA);
        pfn_t lowmem_n = pfn(KERNEL_RESIDENT) - start_pfn;
        lowmem_n = MIN(lowmem_n, size_pg);

        vmm_set_ptes_contig(ptep, pte, lowmem_n, LFT_SIZE);
    }

    /* Reserve region for all loaded modules */
    for (size_t i = 0; i < bhctx->mods.mods_num; i++) {
        struct boot_modent* mod = &bhctx->mods.entries[i];
        pmm_mark_chunk_occupied(PN(mod->start),
                                CEIL(mod->end - mod->start, PG_SIZE_BITS),
                                PP_FGLOCKED);
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

        if (mmapent->start >= KERNEL_RESIDENT || mmapent->type == BOOT_MMAP_FREE) {
            continue;
        }

        ptr_t pa = PG_ALIGN(mmapent->start);
        for (size_t j = 0; j < size_pg && pa < KERNEL_RESIDENT;
             j++, pa += PM_PAGE_SIZE) {
            vmm_del_mapping(VMS_SELF, pa);
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
    // clean up
    for (size_t i = 0; i < (ptr_t)(&__kboot_end); i += PG_SIZE) {
        vmm_del_mapping(VMS_SELF, (ptr_t)i);
        pmm_free_page((ptr_t)i);
    }
}

void
boot_parse_cmdline(struct boot_handoff* bhctx) {
    if (!bhctx->kexec.len) {
        return;
    }

    kcmd_parse_cmdline(bhctx->kexec.cmdline);
}