#include <lunaix/mm/vmm.h>

void* vmm_dup_page(void* va) {    
    void* new_ppg = pmm_alloc_page(KERNEL_PID, 0);
    vmm_fmap_page(KERNEL_PID, PG_MOUNT_3, new_ppg, PG_PREM_RW);

    asm volatile (
        "movl %1, %%edi\n"
        "rep movsl\n"
        :: "c"(1024), "r"(PG_MOUNT_3), "S"((uintptr_t)va)
        : "memory", "%edi");

    vmm_unset_mapping(PG_MOUNT_3);

    return new_ppg;
}