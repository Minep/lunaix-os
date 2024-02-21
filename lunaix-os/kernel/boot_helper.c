#include <klibc/string.h>
#include <lunaix/boot_generic.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <lunaix/kcmd.h>
#include <sys/mm/mm_defs.h>

extern unsigned char __kexec_end[], __kexec_start[];

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
    pte_t pte    = mkpte_prot(KERNEL_DATA);
    size_t count = page_count(KERNEL_RESIDENT, L0T_SIZE);

    vmm_set_ptes_contig(ptep, pte_mkhuge(pte), L0T_SIZE, count);

    // 将内核占据的页，包括前1MB，hhk_init 设为已占用
    pfn_t  kstart = pfn(to_kphysical(__kexec_start));
    size_t pg_count = leaf_count(to_kphysical(__kexec_start));
    pmm_init_freeze_range(kstart, pg_count);

    /* Reserve region for all loaded modules */
    for (size_t i = 0; i < bhctx->mods.mods_num; i++) {
        struct boot_modent* mod = &bhctx->mods.entries[i];
        unsigned int counts = leaf_count(mod->end - mod->start);

        pmm_init_freeze_range(pfn(mod->start), counts);
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