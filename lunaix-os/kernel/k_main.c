#include <stdint.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/kalloc.h>
#include <hal/cpu.h>
#include <libc/stdio.h>
#include <lunaix/spike.h>

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

    // test malloc & free
    
    uint32_t** arr = (uint32_t**) kmalloc(10 * sizeof(uint32_t*));
    
    for (size_t i = 0; i < 10; i++)
    {
        arr[i] = (uint32_t*) kmalloc((i + 1) * 2);
    }


    for (size_t i = 0; i < 10; i++)
    {
        kfree(arr[i]);
    }

    void* big_ = kmalloc(8192);
    
    // good free
    kfree(arr);
    kfree(big_);

    uint8_t* bad1 = kmalloc(123);
    void* bad2 = kmalloc(1);

    *((uint32_t*)(bad1 - 4)) = 0xc2343312UL;

    // bad free
    kfree(bad1);
    kfree(bad2 - 2);
}