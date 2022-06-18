#include <lunaix/mm/vmm.h>

void*
vmm_dup_page(pid_t pid, void* pa)
{
    void* new_ppg = pmm_alloc_page(pid, 0);
    vmm_set_mapping(PD_REFERENCED, PG_MOUNT_3, new_ppg, PG_PREM_RW, VMAP_NULL);
    vmm_set_mapping(PD_REFERENCED, PG_MOUNT_4, pa, PG_PREM_RW, VMAP_NULL);

    asm volatile("movl %1, %%edi\n"
                 "movl %2, %%esi\n"
                 "rep movsl\n" ::"c"(1024),
                 "r"(PG_MOUNT_3),
                 "r"(PG_MOUNT_4)
                 : "memory", "%edi", "%esi");

    vmm_del_mapping(PD_REFERENCED, PG_MOUNT_3);
    vmm_del_mapping(PD_REFERENCED, PG_MOUNT_4);

    return new_ppg;
}