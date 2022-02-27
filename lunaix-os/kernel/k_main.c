#include <stdint.h>
#include <lunaix/mm/vmm.h>
#include <hal/cpu.h>
#include <libc/stdio.h>

extern uint8_t __kernel_start;

void
_kernel_main()
{
    char buf[64];
    
    printf("Hello higher half kernel world!\nWe are now running in virtual "
           "address space!\n\n");
    
    cpu_get_brand(buf);
    printf("CPU: %s\n\n", buf);

    void* k_start = vmm_v2p(&__kernel_start);
    printf("The kernel's base address mapping: %p->%p\n", &__kernel_start, k_start);

    // assert(0);
}