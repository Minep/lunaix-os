#include <lunaix/mm/mmio.h>
#include <lunaix/mm/page.h>
#include <lunaix/spike.h>

ptr_t
ioremap(ptr_t paddr, u32_t size)
{
    // FIXME implement a page policy interface allow to decouple the 
    //       arch-dependent caching behaviour

    pfn_t start = pfn(paddr);
    size_t npages = leaf_count(size);
    
    // Ensure the range is reservable (not already in use)
    assert(pmm_onhold_range(start, npages));

    ptr_t addr = vmap_range(start, npages, KERNEL_DATA);
    return addr + va_offset(paddr);
}

void
iounmap(ptr_t vaddr, u32_t size)
{
    assert(vaddr >= VMAP && vaddr < VMAP_END);
    vunmap_range(pfn(vaddr), leaf_count(size));
}