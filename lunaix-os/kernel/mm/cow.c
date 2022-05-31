#include <lunaix/mm/vmm.h>

void* vmm_dup_page(pid_t pid, void* pa) {    
    void* new_ppg = pmm_alloc_page(pid, 0);
    vmm_fmap_page(pid, PG_MOUNT_3, new_ppg, PG_PREM_RW);
    vmm_fmap_page(pid, PG_MOUNT_4, pa, PG_PREM_RW);

    asm volatile (
        "movl %1, %%edi\n"
        "movl %2, %%esi\n"
        "rep movsl\n"
        :: "c"(1024), "r"(PG_MOUNT_3), "r"(PG_MOUNT_4)
        : "memory", "%edi", "%esi");

    vmm_unset_mapping(PG_MOUNT_3);
    vmm_unset_mapping(PG_MOUNT_4);

    return new_ppg;
}