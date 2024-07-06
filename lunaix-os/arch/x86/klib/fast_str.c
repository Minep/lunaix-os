#include <klibc/string.h>

#ifdef CONFIG_ARCH_X86_64
void*
memcpy(void* dest, const void* src, unsigned long num)
{
    if (!num)
        return dest;
        
    asm volatile("movq %1, %%rdi\n"
                 "rep movsb\n" ::"S"(src),
                 "r"(dest),
                 "c"(num)
                 : "rdi", "memory");
    return dest;
}

void*
memset(void* ptr, int value, unsigned long num)
{
    asm volatile("movq %1, %%rdi\n"
                 "rep stosb\n" ::"c"(num),
                 "r"(ptr),
                 "a"(value)
                 : "rdi", "memory");
    return ptr;
}

#else
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

#endif

