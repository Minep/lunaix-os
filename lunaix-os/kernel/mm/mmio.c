#include <lunaix/mm/mmio.h>
#include <lunaix/mm/page.h>
#include <lunaix/spike.h>

ptr_t
ioremap(ptr_t paddr, u32_t size)
{
    // FIXME implement a page policy interface allow to decouple the 
    //       arch-dependent caching behaviour
    
    pte_t pte;
    ptr_t addr;

    // Ensure the range is reservable (not already in use)
    assert(pmm_onhold_range(page_index(paddr), size / PAGE_SIZE));
    
    addr = vmap_ptes_at(mkpte(paddr, KERNEL_DATA), size / PAGE_SIZE);
    return addr + page_offset(paddr);
}

void
iounmap(ptr_t vaddr, u32_t size)
{
    assert(vaddr >= VMAP && vaddr < VMAP_END);
    vunmap_at(vaddr, size / PAGE_SIZE);
}
