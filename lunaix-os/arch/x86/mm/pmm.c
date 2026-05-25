#include <lunaix/mm/pmm.h>
#include <lunaix/mm/pmalloc.h>
#include <lunaix/mm/vastm.h>
#include <lunaix/spike.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/pagetable.h>
#include <lunaix/sections.h>
#include <lunaix/compiler.h>

#include <asm/tlb.h>
#include <asm/pagetable.h>

#ifdef CONFIG_ARCH_X86_64
// L1T always exists in x86_64 four-level translation scheme

#define PHY_PAGE_MAP_UNIT       L1T_SIZE

#else
#define PHY_PAGE_MAP_UNIT       L0T_SIZE

#endif

static inline void
__map_to_l0t(pte_t* l0tep, ptr_t end)
{
    pte_t pte, *l1t;
    ptr_t paddr;
    int i;

    paddr = 0;
    pte = mkpte_prot(KERNEL_DATA);
    pte = pte_mkhuge(pte);

#ifndef CONFIG_ARCH_X86_64
    while (paddr < end) {
        pte = pte_setpaddr(pte, paddr);
        set_pte(l0tep++, pte);

        paddr += PHY_PAGE_MAP_UNIT;
    }

#else
    /*
     * By this time, we already have page tables in place
     * to house the direct physical memory mappings.
     *
     * We just map everything, including the holes, 
     * to save the reserved page tables and complexity.
     * 
     * TODO store the holes region with separate segment-based 
     * representation and one should use thing like `page_isvalid`
     * to ensure a page is not a hole (looks more like Linux now!)
     */
    while (paddr < end) {
        l1t = (pte_t*)pte_paddr(pte_at(l0tep));
        if (!l1t)
            spin();

        i = 0;
        while (paddr < end && i < L1T_LENGTH) {
            pte = pte_setpaddr(pte, paddr);
            set_pte(&l1t[i], pte);

            i++;
            paddr += PHY_PAGE_MAP_UNIT;
        }

        l0tep++;
    }
#endif
}

static void
remap_phy_pages(struct boot_handoff* bctx)
{
    pte_t pte, *ptep;
    unsigned long mem_max = 0, size;

    pte = pte_mkhuge(mkpte_prot(KERNEL_DATA));
    ptep = (pte_t*)current_vas();
    ptep = &ptep[level_index(mempart(PHYPAGE_POOL), 0)]; 
    
    struct boot_mmapent* ent;
    for_each_boot_mmapent(bctx, ent)
    {
        size = ent->start + ent->size;
        mem_max = size > mem_max ? size : mem_max;
    }

    mem_max = ROUNDUP(mem_max, PHY_PAGE_MAP_UNIT);
    if (mem_max > mempart_size(PHYPAGE_POOL)) {
        mem_max = mempart_size(PHYPAGE_POOL);
    }
    
    __map_to_l0t(ptep, mem_max);
}

static struct pmpool pool_store[2];

void
pmm_arch_init_pool(struct pmem* memory, struct boot_handoff* bctx)
{
    struct pmpool_create_param param;
    
    param.manager = PMALLOC_SIMPLE;

    param.start_addr = 0x100000;
    param.span = memory->list_len - count_pages(0x100000);
    param.container = &pool_store[0];
    pmm_declare_pool(POOL_NORMAL, &param);
    pmm_alias_pool(POOL_NORMAL, POOL_USER);
   
    // Lower 1MiB for legacy 82C37 style DMA pages
    param.start_addr = PAGE_SIZE;
    param.span = count_pages(0x100000) - 1;
    param.container = &pool_store[1];
    pmm_declare_pool(POOL_DMA, &param);
}

ptr_t
pmm_arch_init_remap(struct pmem* memory, struct boot_handoff* bctx)
{
    size_t ppfn_total = count_pages(bctx->mem.size);
    size_t pool_size  = ppfn_total * sizeof(struct ppage);
    
    size_t i = 0;
    struct boot_mmapent* ent;

    remap_phy_pages(bctx);

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
    ptr_t kexec_end = to_kphysical(kernel_start);
    ptr_t aligned_pplist = MAX(ent->start, kexec_end);

#ifdef CONFIG_ARCH_X86_64
    aligned_pplist = ROUNDUP(aligned_pplist, L2T_SIZE);
#else
    aligned_pplist = ROUNDUP(aligned_pplist, L0T_SIZE);
#endif

    if (aligned_pplist + pool_size > ent->start + ent->size) {
        goto restart;
    }

    // for x86_32, the upper bound of memory requirement for pplist
    //  is sizeof(struct ppage) * 1MiB. For simplicity (as well as 
    //  efficiency), we limit the granule to 4M huge page, thus, 
    //  it will take away at least 4M worth of vm address resource 
    //  regardless the actual physical memory size

    memory->pplist = (struct ppage*)mempart(PHYPAGE_MAP);
    memory->list_len = ppfn_total;

    pfn_t nhuge;
    pte_t* ptep;
    pte_t pte   = pte_mkhuge(mkpte_prot(KERNEL_DATA));

#ifdef CONFIG_ARCH_X86_64
    nhuge = ICEIL(pool_size, L2T_SIZE);
    ptep = vastm_walk_ptep(
            vastm_current_root(), mempart(PHYPAGE_MAP), RES_L2T);
    
    set_ptes_level(ptep, pte, aligned_pplist, nhuge, L2T_SIZE);
#else
    nhuge = ICEIL(pool_size, L0T_SIZE);
    ptep = vastm_walk_ptep(
            vastm_current_root(), mempart(PHYPAGE_MAP), RES_L0T);
    
    set_ptes_level(ptep, pte, aligned_pplist, nhuge, L0T_SIZE);
#endif

    tlb_flush_kernel((ptr_t)memory->pplist);
    return aligned_pplist;
}
