#include <klibc/string.h>

void*
memcpy(void* dest, const void* src, unsigned long num)
{
    if (!num)
        return dest;
        
    asm volatile("movl %1, %%edi\n"
                 "rep movsb\n" ::"S"(src),
                 "r"(dest),
                 "c"(num)
                 : "edi", "memory");
    return dest;
}

void*
memset(void* ptr, int value, unsigned long num)
{
    asm volatile("movl %1, %%edi\n"
                 "rep stosb\n" ::"c"(num),
                 "r"(ptr),
                 "a"(value)
                 : "edi", "memory");
    return ptr;
}