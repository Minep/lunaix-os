#include "sys/boot/archinit.h"
#include "sys/crx.h"
#include "sys/cpu.h"

ptr_t __multiboot_addr boot_data;

void boot_text
x86_init(ptr_t mb)
{
    __multiboot_addr = mb;

    cr4_setfeature(CR4_PCIDE);

    ptr_t pagetable = kpg_init();
    cpu_chvmspace(pagetable);

    cr0_unsetfeature(CR0_WP | CR0_EM);
    cr0_setfeature(CR0_MP);
}