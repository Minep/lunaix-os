#include <lunaix/types.h>
#include <klibc/crc.h>

unsigned int
crc32b(unsigned char* data, unsigned int size)
{
    unsigned int register ret asm("x0");

    asm volatile(
        "1:                           \n"
        "ldrb   x2, [%[dest], %[l]]   \n"
        "crc32b %0, %0, x2            \n"
        "sub    %[l], %[l], %1        \n"
        "cbnz   %[l], 1b              \n"
        :
        "=r"(ret)
        :
        "I"(1),
        [dest] "r"(data),
        [l]  "r"(size)
        : "x2"
    );

    return ret;
}
