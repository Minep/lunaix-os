#include <lunaix/types.h>
#include <klibc/crc.h>

#ifdef CONFIG_X86_ENABLE_SSE_FEATURE
unsigned int
crc32b(unsigned char* data, unsigned int size)
{
    unsigned int ret;
    asm volatile(
        "xorl %%ebx, %%ebx\n"
        "xorl %%eax, %%eax\n"
        "1:\n"
        "crc32 (%%edx, %%ebx, 1), %%eax\n"
        "incl %%ebx\n"
        "cmpl %%ebx, %%ecx\n"
        "jne 1b\n"
        : "=a"(ret)
        : "d"((ptr_t)data),
          "c"(size)
        :
    );
    return ret;
}
#endif