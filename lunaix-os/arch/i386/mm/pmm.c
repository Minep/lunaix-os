#include <lunaix/mm/page.h>
#include <lunaix/mm/pagetable.h>

extern unsigned int __kexec_end[];

void
pmm_arch_init_pool(struct pmem* memory)
{
    pmm_declare_pool(POOL_UNIFIED, 1, memory->list_len);
}

ptr_t
pmm_arch_init_remap(struct pmem* memory, struct boot_handoff* bctx)
{
    size_t ppfn_total = pfn(bctx->mem.size) + 1;
    size_t pool_size  = ppfn_total * sizeof(struct ppage);
    
    size_t i = 0;
    struct boot_mmapent* ent;
    for (; i < bctx->mem.mmap_len; i++) {
        ent = &bctx->mem.mmap[i];
        if (free_memregion(ent) && ent->size > pool_size) {
            goto found;
        }
    }

    // fail to find a viable free region to host pplist
    return 0;

found:;
    ptr_t kexec_end = to_kphysical(__kexec_end);
    ptr_t aligned_pplist = MAX(ent->start, kexec_end);

    aligned_pplist = napot_upaligned(aligned_pplist, L0T_SIZE);

    if (aligned_pplist + pool_size > ent->start + ent->size) {
        return 0;
    }

    // for x86_32, the upper bound of memory requirement for pplist
    //  is sizeof(struct ppage) * 1MiB. For simplicity (as well as 
    //  efficiency), we limit the granule to 4M huge page, thus, 
    //  it will take away at least 4M worth of vm address resource 
    //  regardless the actual physical memory size

    // anchor the pplist at vmap location (right after kernel)
    memory->pplist = (struct ppage*)VMAP;
    memory->list_len = ppfn_total;

    pfn_t nhuge = page_count(pool_size, L0T_SIZE);
    pte_t* ptep = mkl0tep_va(VMS_SELF, VMAP);
    pte_t pte   = mkpte(aligned_pplist, KERNEL_DATA);
    
    vmm_set_ptes_contig(ptep, pte_mkhuge(pte), L0T_SIZE, nhuge);
    tlb_flush_kernel(VMAP);

    // shift the actual vmap start address
    vmap_set_start(VMAP + nhuge * L0T_SIZE);

    return aligned_pplist;
}