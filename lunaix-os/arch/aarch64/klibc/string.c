#include <klibc/string.h>
#include <lunaix/compiler.h>

void*
memcpy(void* dest, const void* src, unsigned long num)
{
    if (unlikely(!num))
        return dest;
        
    asm volatile(
        "1:                         \n"
        "ldrb x0, [%[src],  %[l]]   \n"
        "strb x0, [%[dest], %[l]]   \n"
        "sub  %[l], %[l], %0        \n"
        "cbnz  %[l], 1b             \n"
        ::
        "I"(1),
        [src]  "r"(dest),
        [dest] "r"(src),
        [l]    "r"(num)
        : "x0" "x1"
    );

    return dest;
}

void*
memset(void* ptr, int value, unsigned long num)
{
    asm volatile(
        "1:                             \n"
        "strb %[val], [%[dest], %[l]]   \n"
        "sub  %[l], %[l], %0            \n"
        "cbnz %[l], 1b                  \n"
        ::
        "I"(1),
        [val]  "r"(value),
        [dest] "r"(ptr),
        [l]    "r"(num)
        : "x1"
    );

    return ptr;
}

unsigned long
strlen(const char* str)
{
    unsigned long register _c asm("x0");

    _c = 0;
    asm volatile(
        "1:                     \n"
        "ldrb x1, [%[ptr], %0]  \n"
        "add  %0, %0, %1        \n"
        "cbnz %0, 1b            \n"
        :
        "=r"(_c)
        :
        "I"(1),
        [ptr]  "r"(str)
        : "x1"
    );

    return _c;
}

unsigned long
strnlen(const char* str, unsigned long max_len)
{
    unsigned long register _c asm("x0");

    _c = 0;
    asm volatile(
        "1:                     \n"
        "ldrb x1, [%[ptr], %0]  \n"
        "add  %0, %0, %1        \n"
        "sub  x2, %[len], %0    \n"
        "ands x2, x2, x1        \n"
        "bne  1b                \n"
        :
        "=r"(_c)
        :
        "I"(1),
        [ptr]  "r"(str),
        [len]  "r"(max_len)
        : "x1", "x2"
    );

    return _c;
}