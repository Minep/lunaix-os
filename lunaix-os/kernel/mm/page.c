#include <lunaix/mm/page.h>

pte_t 
alloc_kpage_at(pte_t* ptep, pte_t pte, int order)
{
    ptr_t va = ptep_va(ptep, LFT_SIZE);

    assert(kernel_addr(va));

    struct leaflet* leaflet = alloc_leaflet_pinned(order);

    if (!leaflet) {
        return null_pte;
    }

    leaflet_wipe(leaflet);

    ptep_map_leaflet(ptep, pte, leaflet);

    tlb_flush_kernel_ranged(va, leaflet_nfold(leaflet));

    return pte_at(ptep);
}