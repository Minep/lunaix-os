#include <klibc/string.h>
#include <lunaix/types.h>

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
memmove(void* dest, const void* src, unsigned long num)
{
    u8_t* dest_ptr = (u8_t*)dest;
    const u8_t* src_ptr = (const u8_t*)src;
    if (dest_ptr < src_ptr) {
        for (unsigned long i = 0; i < num; i++) {
            *(dest_ptr + i) = *(src_ptr + i);
        }
    } else {
        for (unsigned long i = num; i != 0; i--) {
            *(dest_ptr + i - 1) = *(src_ptr + i - 1);
        }
    }
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

int
memcmp(const void* ptr1, const void* ptr2, unsigned long num)
{
    u8_t* p1 = (u8_t*)ptr1;
    u8_t* p2 = (u8_t*)ptr2;
    for (unsigned long i = 0; i < num; i++) {
        int diff = *(p1 + i) - *(p2 + i);
        if (diff != 0) {
            return diff;
        }
    }
    return 0;
}