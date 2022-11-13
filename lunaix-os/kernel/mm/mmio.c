#include <lunaix/mm/mmio.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

void*
ioremap(uintptr_t paddr, u32_t size)
{
    void* ptr = vmm_vmap(paddr, size, PG_PREM_RW | PG_DISABLE_CACHE);
    if (ptr) {
        pmm_mark_chunk_occupied(KERNEL_PID,
                                paddr >> PG_SIZE_BITS,
                                CEIL(size, PG_SIZE_BITS),
                                PP_FGLOCKED);
    }
    return ptr;
}

void*
iounmap(uintptr_t vaddr, u32_t size)
{
    for (size_t i = 0; i < size; i += PG_SIZE) {
        uintptr_t paddr = vmm_del_mapping(PD_REFERENCED, vaddr + i);
        pmm_free_page(KERNEL_PID, paddr);
    }
}