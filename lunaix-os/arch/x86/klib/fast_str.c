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

unsigned long
strlen(const char* str)
{
    unsigned long _c;
    asm volatile("movq %1, %%rdi\n"
                 "movq $-1, %%rcx\n"
                 "repne scasb\n"
                 "neg %%rcx\n"
                 "movq %%rcx, %0\n"
                 : "=r" (_c)
                 : "r"(str), "a"(0)
                 : "rdi", "rcx", "memory");

    return _c - 2;
}

unsigned long
strnlen(const char* str, unsigned long max_len)
{
    unsigned long _c;
    asm volatile("movq %1, %%rdi\n"
                 "movq %2, %%rcx\n"
                 "repne scasb\n"
                 "movq %%rcx, %0\n"
                 : "=r" (_c)
                 : "r"(str), "r"(max_len), "a"(0)
                 : "rdi", "rcx", "memory");

    return max_len - _c - 1;
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

