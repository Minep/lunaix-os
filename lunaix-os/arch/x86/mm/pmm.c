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

restart:;
    while (i < bctx->mem.mmap_len) {
        ent = &bctx->mem.mmap[i++];
        if (free_memregion(ent) && ent->size > pool_size) {
            goto found;
        }
    }

    // fail to find a viable free region to host pplist
    return 0;

found:;
    ptr_t kexec_end = to_kphysical(__kexec_end);
    ptr_t aligned_pplist = MAX(ent->start, kexec_end);

#ifdef CONFIG_ARCH_X86_64
    aligned_pplist = napot_upaligned(aligned_pplist, L2T_SIZE);
#else
    aligned_pplist = napot_upaligned(aligned_pplist, L0T_SIZE);
#endif

    if (aligned_pplist + pool_size > ent->start + ent->size) {
        goto restart;
    }

    // for x86_32, the upper bound of memory requirement for pplist
    //  is sizeof(struct ppage) * 1MiB. For simplicity (as well as 
    //  efficiency), we limit the granule to 4M huge page, thus, 
    //  it will take away at least 4M worth of vm address resource 
    //  regardless the actual physical memory size

    // anchor the pplist at vmap location (right after kernel)
    memory->pplist = (struct ppage*)PMAP;
    memory->list_len = ppfn_total;

    pfn_t nhuge;
    pte_t* ptep;

    pte_t pte   = mkpte(aligned_pplist, KERNEL_DATA);

#ifdef CONFIG_ARCH_X86_64
    nhuge = page_count(pool_size, L2T_SIZE);
    ptep = mkl2tep_va(VMS_SELF, PMAP);
    vmm_set_ptes_contig(ptep, pte_mkhuge(pte), L2T_SIZE, nhuge);
#else
    nhuge = page_count(pool_size, L0T_SIZE);
    ptep = mkl0tep_va(VMS_SELF, PMAP);
    
    // since VMAP and PMAP share same address space
    // we need to shift VMAP to make room
    vmap_set_start(VMAP + nhuge * L0T_SIZE);
    vmm_set_ptes_contig(ptep, pte_mkhuge(pte), L0T_SIZE, nhuge);
#endif

    tlb_flush_kernel(PMAP);
    return aligned_pplist;
}