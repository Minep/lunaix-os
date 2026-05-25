#ifndef __LUNAIX_PAGE_H
#define __LUNAIX_PAGE_H

#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/pgpol.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/spike.h>

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

#define foreach_page(head_page, _page) \
    for (int i = 0; \
             (_page = &head_page[i]) && i < (1 << ppage_order(head_page)); \
             i++)

#define foreach_page_in_leaflet(leaflet, page) \
    foreach_page(get_ppage(leaflet), page)

static inline bool
reserved_page(struct ppage* page)
{
    return page->pol == __PGPOL_RESERVED;
}

static inline struct ppage*
ppage(pfn_t pfn) 
{
    return (struct ppage*)(PPLIST_STARTVM) + pfn;
}

static inline struct ppage*
leading_page(struct ppage* page) {
    return page - page->companion;
}

static inline struct ppage*
ppage_of(struct pmpool* pool, pfn_t pfn) 
{
    return pool->first + pfn;
}

static inline pfn_t
ppfn(struct ppage* page) 
{
    return (pfn_t)((ptr_t)page - PPLIST_STARTVM) / sizeof(struct ppage);
}

static inline pfn_t
ppfn_of(struct pmpool* pool, struct ppage* page) 
{
    return (pfn_t)((ptr_t)page - (ptr_t)pool->first) / sizeof(struct ppage);
}

static inline ptr_t
ppage_addr(struct ppage* page) {
    return ppfn(page) * PAGE_SIZE;
}

static inline unsigned int
count_order(size_t page_count) {
    unsigned int po = ilog2(page_count);
    assert(!(page_count % (1 << po)));
    return po;
}

static inline unsigned int
ppage_order(struct ppage* page) {
    return page->order;
}

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

static inline pgpol_t
leaflet_policy(struct leaflet* leaflet)
{
    return leaflet->lead_page.pol;
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
    return phy_to_virt(ppfn(page) * PAGE_SIZE);
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

static inline size_t
count_pages(size_t size) 
{
    return CEIL(size, PAGE_SHIFT);
}

struct leaflet*
leaflet_alloc_order(pgpol_t alloc_pol, int order);

static inline struct leaflet*
alloc_leaflet(pgpol_t alloc_pol)
{
    return leaflet_alloc_order(alloc_pol, 0);
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
page_return(struct ppage* page)
{
    assert(page->refs);
    pmm_free_one(page);
}

static inline void
leaflet_return(struct leaflet* leaflet)
{
    page_return(get_ppage(leaflet));
}

#endif /* __LUNAIX_PAGE_H */
