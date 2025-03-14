#ifndef __LUNAIX_PAGE_H
#define __LUNAIX_PAGE_H

#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/vmtlb.h>

#include <klibc/string.h>

/**
 * @brief A leaflet represent a bunch 4k ppage
 *        as single multi-ordered page, as such
 *        big page can seen as an unfolded version
 *        of these small 4k ppages hence the name.
 *        It is introduced to solve the issue that
 *        is discovered during refactoring - It is 
 *        jolly unclear whether the ppage is a head, 
 *        tail, or even worse, the middle one, when
 *        passing around between functions.
 *        This concept is surprisingly similar to
 *        Linux's struct folio (I swear to the 
 *        Almighty Princess of the Sun, Celestia, 
 *        that I don't quite understand what folio 
 *        is until I've wrote the conceptually same 
 *        thing)
 * 
 */
struct leaflet
{
    struct ppage lead_page;
};

static inline struct leaflet*
get_leaflet(struct ppage* page)
{
    return (struct leaflet*)leading_page(page);
}

static inline struct ppage*
get_ppage(struct leaflet* leaflet)
{
    return (struct ppage*)leaflet;
}

static inline struct leaflet*
alloc_leaflet(int order)
{
    return (struct leaflet*)pmm_alloc_napot_type(POOL_UNIFIED, order, 0);
}

static inline struct leaflet*
alloc_leaflet_pinned(int order)
{
    return (struct leaflet*)pmm_alloc_napot_type(POOL_UNIFIED, order, PP_FGLOCKED);
}

static inline void
leaflet_borrow(struct leaflet* leaflet)
{
    struct ppage* const page = get_ppage(leaflet);
    assert(page->refs);
    if (reserved_page(page)) {
        return;
    }
    
    page->refs++;
}

static inline void
leaflet_return(struct leaflet* leaflet)
{
    struct ppage* const page = get_ppage(leaflet);
    assert(page->refs);
    pmm_free_one(page, 0);
}

static inline unsigned int
leaflet_refcount(struct leaflet* leaflet)
{
    return get_ppage(leaflet)->refs;
}

static inline int
leaflet_order(struct leaflet* leaflet)
{
    return ppage_order(get_ppage(leaflet));
}

static inline int
leaflet_size(struct leaflet* leaflet)
{
    return PAGE_SIZE << leaflet_order(leaflet);
}

static inline int
leaflet_nfold(struct leaflet* leaflet)
{
    return 1 << leaflet_order(leaflet);
}

static inline struct leaflet*
ppfn_leaflet(pfn_t ppfn)
{
    return get_leaflet(ppage(ppfn));
}

static inline struct leaflet*
pte_leaflet(pte_t pte)
{
    struct ppage* ppfn = ppage(pfn(pte_paddr(pte)));
    return get_leaflet(ppfn);
}

static inline struct leaflet*
pte_leaflet_aligned(pte_t pte)
{
    struct ppage* ppfn = ppage(pfn(pte_paddr(pte)));
    struct leaflet* _l = get_leaflet(ppfn);

    assert((ptr_t)_l == (ptr_t)ppfn);
    return _l;
}

static inline pfn_t
leaflet_ppfn(struct leaflet* leaflet)
{
    return ppfn(get_ppage(leaflet));
}

static inline ptr_t
leaflet_addr(struct leaflet* leaflet)
{
    return page_addr(ppfn(get_ppage(leaflet)));
}

static inline void
unpin_leaflet(struct leaflet* leaflet)
{
    change_page_type(get_ppage(leaflet), 0);
}

static inline void
pin_leaflet(struct leaflet* leaflet)
{
    change_page_type(get_ppage(leaflet), PP_FGLOCKED);
}

static inline int
to_napot_order(int nr_pages)
{
    int order;
    
    order = ilog2(nr_pages);
    return is_pot(nr_pages) ? order : order + 1;
}

/**
 * @brief Map a leaflet
 * 
 * @param ptep 
 * @param leaflet 
 * @return pages folded into that leaflet
 */
static inline size_t
ptep_map_leaflet(pte_t* ptep, pte_t pte, struct leaflet* leaflet)
{
    // We do not support huge leaflet yet
    assert(leaflet_order(leaflet) < LEVEL_SHIFT);

    pte = pte_setppfn(pte, leaflet_ppfn(leaflet));
    pte = pte_mkloaded(pte);

    int n = leaflet_nfold(leaflet);
    vmm_set_ptes_contig(ptep, pte, LFT_SIZE, n);

    return n;
}

/**
 * @brief Unmap a leaflet
 * 
 * @param ptep 
 * @param leaflet 
 * @return pages folded into that leaflet
 */
static inline size_t
ptep_unmap_leaflet(pte_t* ptep, struct leaflet* leaflet)
{
    // We do not support huge leaflet yet
    assert(leaflet_order(leaflet) < LEVEL_SHIFT);

    int n = leaflet_nfold(leaflet);
    vmm_unset_ptes(ptep, n);

    return n;
}

static inline ptr_t
leaflet_mount(struct leaflet* leaflet)
{
    pte_t* ptep = mkptep_va(VMS_SELF, PG_MOUNT_VAR);    
    ptep_map_leaflet(ptep, mkpte_prot(KERNEL_DATA), leaflet);

    tlb_flush_kernel_ranged(PG_MOUNT_VAR, leaflet_nfold(leaflet));

    return PG_MOUNT_VAR;
}

static inline void
leaflet_unmount(struct leaflet* leaflet)
{
    pte_t* ptep = mkptep_va(VMS_SELF, PG_MOUNT_VAR);    
    vmm_unset_ptes(ptep, leaflet_nfold(leaflet));

    tlb_flush_kernel_ranged(PG_MOUNT_VAR, leaflet_nfold(leaflet));
}

static inline void
leaflet_fill(struct leaflet* leaflet, unsigned int val)
{
    ptr_t mnt;
    
    mnt = leaflet_mount(leaflet);
    memset((void*)mnt, val, leaflet_size(leaflet));
    leaflet_unmount(leaflet);
}

static inline void
leaflet_wipe(struct leaflet* leaflet)
{
    leaflet_fill(leaflet, 0);
}

/**
 * @brief Duplicate the leaflet
 *
 * @return Duplication of given leaflet
 *
 */
struct leaflet*
dup_leaflet(struct leaflet* leaflet);


/**
 * @brief Maps a number of contiguous ptes in kernel 
 *        address space
 * 
 * @param pte the pte to be mapped
 * @param lvl_size size of the page pointed by the given pte
 * @param n number of ptes
 * @return ptr_t 
 */
ptr_t
vmap_ptes_at(pte_t pte, size_t lvl_size, int n);

/**
 * @brief Maps a number of contiguous ptes in kernel 
 *        address space (leaf page size)
 * 
 * @param pte the pte to be mapped
 * @param n number of ptes
 * @return ptr_t 
 */
static inline ptr_t
vmap_leaf_ptes(pte_t pte, int n)
{
    return vmap_ptes_at(pte, LFT_SIZE, n);
}

/**
 * @brief Maps a contiguous range of physical address 
 *        into kernel address space (leaf page size)
 * 
 * @param paddr start of the physical address range
 * @param size size of the physical range
 * @param prot default protection to be applied
 * @return ptr_t 
 */
static inline ptr_t
vmap(struct leaflet* leaflet, pte_attr_t prot)
{
    pte_t _pte = mkpte(page_addr(leaflet_ppfn(leaflet)), prot);
    return vmap_ptes_at(_pte, LFT_SIZE, leaflet_nfold(leaflet));
}

void
vunmap(ptr_t ptr, struct leaflet* leaflet);

static inline ptr_t
vmap_range(pfn_t start, size_t npages, pte_attr_t prot)
{
    pte_t _pte = mkpte(page_addr(start), prot);
    return vmap_ptes_at(_pte, LFT_SIZE, npages);
}

static inline void
vunmap_range(pfn_t start, size_t npages)
{
    pte_t* ptep = mkptep_va(VMS_SELF, start);
    vmm_set_ptes_contig(ptep, null_pte, LFT_SIZE, npages);
}


/**
 * @brief Allocate a page in kernel space.
 * 
 * @param ptep 
 * @param pte 
 * @param order 
 * @return pte_t 
 */
pte_t 
alloc_kpage_at(pte_t* ptep, pte_t pte, int order);

static inline void*
vmalloc_page(int order)
{
    struct leaflet* leaf = alloc_leaflet(0);
    if (!leaf) {
        return NULL;
    }

    return (void*)vmap(leaf, KERNEL_DATA);
}

static inline void
vmfree(void* ptr)
{
    struct leaflet* leaf = ppfn_leaflet(pfn((ptr_t)ptr));
    leaflet_return(leaf);
}

#endif /* __LUNAIX_PAGE_H */
