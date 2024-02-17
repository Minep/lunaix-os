#include <lunaix/mm/vmm.h>
#include <sys/mm/mm_defs.h>

ptr_t
vmm_dup_page(ptr_t pa)
{
    ptr_t new_ppg = pmm_alloc_page(0);
    mount_page(PG_MOUNT_3, new_ppg);
    mount_page(PG_MOUNT_4, pa);

    asm volatile("movl %1, %%edi\n"
                 "movl %2, %%esi\n"
                 "rep movsl\n" ::"c"(1024),
                 "r"(PG_MOUNT_3),
                 "r"(PG_MOUNT_4)
                 : "memory", "%edi", "%esi");

    unmount_page(PG_MOUNT_3);
    unmount_page(PG_MOUNT_4);

    return new_ppg;
}