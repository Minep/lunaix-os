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
    
    uint8_t** arr = (uint8_t**) lxmalloc(10 * sizeof(uint8_t*));
    
    for (size_t i = 0; i < 10; i++)
    {
        arr[i] = (uint8_t*) lxmalloc((i + 1) * 2);
    }


    for (size_t i = 0; i < 10; i++)
    {
        lxfree(arr[i]);
    }

    uint8_t* big_ = lxmalloc(8192);
    big_[0] = 123;
    big_[1] = 23;
    big_[2] = 3;

    printf("%u, %u, %u", big_[0], big_[1], big_[2]);
    
    // good free
    lxfree(arr);
    lxfree(big_);

    // uint8_t* bad1 = lxmalloc(123);
    // void* bad2 = lxmalloc(1);

    // *((uint32_t*)(bad1 - 4)) = 0xc2343312UL;

    // // bad free
    // lxfree(bad1);
    // lxfree(bad2 - 2);
}