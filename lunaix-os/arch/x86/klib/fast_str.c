#include <klibc/string.h>

#ifdef CONFIG_ARCH_X86_64
#define DEST_REG "%%rdi"
#else
#define DEST_REG "%%edi"
#endif

void*
memcpy(void* dest, const void* src, unsigned long num)
{
    if (!num)
        return dest;
        
    asm volatile("movl %1, " DEST_REG "\n"
                 "rep movsb\n" ::"S"(src),
                 "r"(dest),
                 "c"(num)
                 : "di", "memory");
    return dest;
}

void*
memset(void* ptr, int value, unsigned long num)
{
    asm volatile("movl %1, " DEST_REG "\n"
                 "rep stosb\n" ::"c"(num),
                 "r"(ptr),
                 "a"(value)
                 : "di", "memory");
    return ptr;
}