#include <lunaix/mm/mmio.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

void*
ioremap(ptr_t paddr, u32_t size)
{
    // FIXME implement a page policy interface allow to decouple the 
    //       arch-dependent caching behaviour
    void* ptr = (void*)vmap(paddr, size, PG_PREM_RW);

    if (ptr) {
        pmm_mark_chunk_occupied(paddr >> PG_SIZE_BITS,
                                CEIL(size, PG_SIZE_BITS),
                                PP_FGLOCKED);
    }

    return ptr;
}

void
iounmap(ptr_t vaddr, u32_t size)
{
    for (size_t i = 0; i < size; i += PG_SIZE) {
        ptr_t paddr = vmm_del_mapping(VMS_SELF, vaddr + i);
        pmm_free_page(paddr);
    }
}