#include <hal/cpu.h>
#include <hal/rtc.h>
#include <lunaix/syslog.h>
#include <lunaix/mm/kalloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <stdint.h>

extern uint8_t __kernel_start;

LOG_MODULE("LX")

void
_kernel_main()
{
    char buf[64];

    kprintf(KINFO "Hello higher half kernel world!\nWe are now running in virtual "
           "address space!\n\n");

    cpu_get_brand(buf);
    kprintf("CPU: %s\n\n", buf);

    void* k_start = vmm_v2p(&__kernel_start);
    kprintf(KINFO "The kernel's base address mapping: %p->%p\n", &__kernel_start, k_start);

    // test malloc & free

    uint8_t** arr = (uint8_t**)lxmalloc(10 * sizeof(uint8_t*));

    for (size_t i = 0; i < 10; i++) {
        arr[i] = (uint8_t*)lxmalloc((i + 1) * 2);
    }

    for (size_t i = 0; i < 10; i++) {
        lxfree(arr[i]);
    }

    uint8_t* big_ = lxmalloc(8192);
    big_[0] = 123;
    big_[1] = 23;
    big_[2] = 3;

    kprintf(KINFO "%u, %u, %u\n", big_[0], big_[1], big_[2]);

    // good free
    lxfree(arr);
    lxfree(big_);

    spin();
}