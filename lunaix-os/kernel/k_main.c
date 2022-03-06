#include <hal/cpu.h>
#include <hal/rtc.h>
#include <libc/stdio.h>
#include <lunaix/mm/kalloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <stdint.h>

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
    printf(
      "The kernel's base address mapping: %p->%p\n", &__kernel_start, k_start);

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

    printf("%u, %u, %u\n", big_[0], big_[1], big_[2]);

    // good free
    lxfree(arr);
    lxfree(big_);

    rtc_datetime datetime;

    rtc_get_datetime(&datetime);

    printf("%u/%u/%u %u:%u:%u",
           datetime.year,
           datetime.month,
           datetime.day,
           datetime.hour,
           datetime.minute,
           datetime.second);
}