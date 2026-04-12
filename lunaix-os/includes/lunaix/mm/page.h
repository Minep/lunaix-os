#ifndef __LUNAIX_PAGE_H
#define __LUNAIX_PAGE_H

#include <lunaix/mm/pmm.h>

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
    struct ppage* ppfn = ppage(page_index(pte_paddr(pte)));
    return get_leaflet(ppfn);
}

static inline pfn_t
leaflet_ppfn(struct leaflet* leaflet)
{
    return ppfn(get_ppage(leaflet));
}

static inline ptr_t
leaflet_addr(struct leaflet* leaflet)
{
    return ppage_addr(get_ppage(leaflet));
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
vmap_ptes_at(pte_t pte, int n);

void
vunmap_at(ptr_t vmap_addr, int n);

static inline ptr_t
leaflet_va(struct leaflet* leaflet)
{
    if (unlikely(!leaflet))
        return 0;
    return phy_to_virt(leaflet_ppfn(leaflet) * PAGE_SIZE);
}

static inline struct leaflet*
leaflet_from_va(ptr_t va)
{
    return ppfn_leaflet(virt_to_phy(va) / PAGE_SIZE);
}

static inline ptr_t
ppage_va(struct ppage* page)
{
    if (unlikely(!page))
        return 0;
    return phy_to_virt(ppfn(page));
}

static inline struct ppage*
ppage_from_va(ptr_t va)
{
    return ppage(virt_to_phy(va) / PAGE_SIZE);
}

static inline void
leaflet_fill(struct leaflet* leaflet, unsigned int val)
{
    ptr_t va = leaflet_va(leaflet);
    memset((void*)va, val, leaflet_size(leaflet));
}

static inline void
leaflet_wipe(struct leaflet* leaflet)
{
    leaflet_fill(leaflet, 0);
}

static inline struct leaflet*
alloc_leaflet(int order)
{
    return (struct leaflet*)pmm_alloc_napot_type(POOL_UNIFIED, order, 0);
}

static inline struct ppage*
alloc_page()
{
    return pmm_alloc_napot_type(POOL_UNIFIED, 0, 0);
}

static inline struct leaflet*
alloc_leaflet_pinned(int order)
{
    return (struct leaflet*)pmm_alloc_napot_type(POOL_UNIFIED, order, PP_FGLOCKED);
}

static inline struct leaflet*
alloc_page_table()
{
    struct leaflet* leaflet = alloc_leaflet_pinned(0);
    if (!leaflet)
        return NULL;

    leaflet_wipe(leaflet);
    return leaflet;
}

static inline size_t
count_pages(size_t size) 
{
    return ROUNDUP(size, PAGE_SIZE);
}
#endif /* __LUNAIX_PAGE_H */
