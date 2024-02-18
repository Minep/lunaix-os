#include <lunaix/mm/mmio.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

void*
ioremap(ptr_t paddr, u32_t size)
{
    // FIXME implement a page policy interface allow to decouple the 
    //       arch-dependent caching behaviour
    void* ptr = (void*)vmap(paddr, size, KERNEL_DATA);

    if (ptr) {
        pmm_mark_chunk_occupied(pfn(paddr), leaf_count(size), PP_FGLOCKED);
    }

    return ptr;
}

void
iounmap(ptr_t vaddr, u32_t size)
{
    pte_t* ptep = mkptep_va(VMS_SELF, vaddr);
    for (size_t i = 0; i < size; i += PAGE_SIZE, ptep++) {
        pte_t pte = pte_at(ptep);

        set_pte(ptep, null_pte);
        if (pte_isloaded(pte))
            pmm_free_page(pte_paddr(pte));
    }
}