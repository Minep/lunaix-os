#include <lunaix/mm/mmio.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>

void*
ioremap(uintptr_t paddr, uint32_t size)
{
    return vmm_vmap(paddr, size, PG_PREM_RW | PG_DISABLE_CACHE);
}

void*
iounmap(uintptr_t vaddr, uint32_t size)
{
    for (size_t i = 0; i < size; i += PG_SIZE) {
        uintptr_t paddr = vmm_del_mapping(PD_REFERENCED, vaddr + i);
        pmm_free_page(KERNEL_PID, paddr);
    }
}