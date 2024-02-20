#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/pagetable.h>

void
pmm_arch_init_pool(struct pmem* memory)
{
    struct pmem_pool* pool = &memory->pool[POOL_UNIFIED];

    pool->type = POOL_UNIFIED;
    pool->pool_end = &memory->pplist[memory->size - 1];
    pool->pool_start = &memory->pplist[1];   // skip first page

    pmm_build_free_list(pool);
}

void
pmm_arch_init_begin(struct pmem* memory, struct boot_handoff* bctx)
{
    memory->state = PMM_IN_INIT;

    size_t ppfn_total = pfn(bctx->mem.size) + 1;
    size_t pool_size  = ppfn_total * sizeof(struct ppage);
    
    struct boot_mmapent* ent;
    for (int i = 0; i < bctx->mem.mmap_len; i++) {
        ent = &bctx->mem.mmap[i];
        if (free_memregion(ent) && ent->size > pool_size) {
            goto found;
        }
    }

    // fail to find a viable free region to host pplist
    spin();

found:;
    ptr_t aligned_pplist = page_upaligned(ent->start);

    // for x86_32, the upper bound of memory requirement for pplist
    //  is sizeof(struct ppage) * 1MiB. For simplicity (as well as 
    //  efficiency), we limit the granule to 4M huge page, thus, 
    //  it will take away at least 4M worth of vm address resource 
    //  regardless the actual physical memory size

    pfn_t nhuge = page_count(pool_size, L0T_SIZE);
    pte_t* ptep = mkl0tep_va(VMS_SELF, VMAP);
    pte_t pte   = mkpte(aligned_pplist, KERNEL_DATA);
    
    vmm_set_ptes_contig(ptep, pte_mkhuge(pte), L0T_SIZE, nhuge);

    memory->pplist = aligned_pplist;
    memory->size = ppfn_total;

    pmm_init_freeze_range(aligned_pplist, leaf_count(pool_size));

    vmap_set_start(VMAP + nhuge * L0T_SIZE);
}