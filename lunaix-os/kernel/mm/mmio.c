#include "lunaix/syslog.h"
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/mmio.h>
#include <lunaix/mm/page.h>
#include <lunaix/spike.h>

LOG_MODULE("iomem")

ptr_t
ioremap(ptr_t paddr, u32_t size)
{
    // FIXME implement a page policy interface allow to decouple the 
    //       arch-dependent caching behaviour
    
    pte_t pte;
    ptr_t addr;
    int err;
    
    /* XXX
     * Identially, any ad-hoc ioremap should go through iommu if possible,
     * and direct reservation should only happened in early boot.
     */

    // ensure mmio pages are taken out from any pool
    //
    err = pmm_onhold_range(page_index(paddr), size / PAGE_SIZE);
    if (err) {
        ERROR("unable to reserve for ioremap at 0x%lx. (%d)",
                paddr, err);
        return 0;
    }
    
    addr = vmap_ptes_at(mkpte(paddr, KERNEL_DATA), size / PAGE_SIZE);
    return addr + page_offset(paddr);
}

void
iounmap(ptr_t vaddr, u32_t size)
{
    assert(vaddr >= VMAP && vaddr < VMAP_END);
    vunmap_at(vaddr, size / PAGE_SIZE);
}
