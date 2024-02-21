#include <lunaix/mm/mmio.h>
#include <lunaix/mm/page.h>
#include <lunaix/spike.h>

void*
ioremap(ptr_t paddr, u32_t size)
{
    // FIXME implement a page policy interface allow to decouple the 
    //       arch-dependent caching behaviour

    pfn_t start = pfn(paddr);
    size_t npages = leaf_count(paddr);
    
    // Ensure the range is reservable (not already in use)
    assert(pmm_onhold_range(start, npages));

    return (void*)vmap_range(start, npages, KERNEL_DATA);
}

void
iounmap(ptr_t vaddr, u32_t size)
{
    // FIXME
    fail("need fix");

    // pte_t* ptep = mkptep_va(VMS_SELF, vaddr);
    // for (size_t i = 0; i < size; i += PAGE_SIZE, ptep++) {
    //     pte_t pte = pte_at(ptep);

    //     set_pte(ptep, null_pte);
    //     if (pte_isloaded(pte))
    //         return_page(ppage_pa(pte_paddr(pte)));
    // }
}