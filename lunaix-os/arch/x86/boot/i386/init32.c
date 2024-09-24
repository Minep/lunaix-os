#include "sys/boot/archinit.h"
#include "asm/x86_cpu.h"

ptr_t __multiboot_addr boot_data;

void boot_text
x86_init(ptr_t mb)
{
    __multiboot_addr = mb;

    cr4_setfeature(CR4_OSXMMEXCPT | CR4_OSFXSR | CR4_PSE36);

    ptr_t pagetable = kpg_init();
    cpu_chvmspace(pagetable);

    cr0_unsetfeature(CR0_WP | CR0_EM);
    cr0_setfeature(CR0_PG | CR0_MP);
}