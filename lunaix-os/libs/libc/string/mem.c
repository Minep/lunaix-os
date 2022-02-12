#include <stdint.h>
#include <libc/string.h>

void*
memcpy(void* dest, const void* src, size_t num)
{
    uint8_t* dest_ptr = (uint8_t*)dest;
    const uint8_t* src_ptr = (const uint8_t*)src;
    for (size_t i = 0; i < num; i++) {
        *(dest_ptr + i) = *(src_ptr + i);
    }
    return dest;
}

void*
memmove(void* dest, const void* src, size_t num)
{
    uint8_t* dest_ptr = (uint8_t*)dest;
    const uint8_t* src_ptr = (const uint8_t*)src;
    if (dest_ptr < src_ptr) {
        for (size_t i = 0; i < num; i++) {
            *(dest_ptr + i) = *(src_ptr + i);
        }
    } else {
        for (size_t i = num; i != 0; i--) {
            *(dest_ptr + i - 1) = *(src_ptr + i - 1);
        }
    }
    return dest;
}

void*
memset(void* ptr, int value, size_t num)
{
    uint8_t* c_ptr = (uint8_t*)ptr;
    for (size_t i = 0; i < num; i++) {
        *(c_ptr + i) = (uint8_t)value;
    }
    return ptr;
}

int
memcmp(const void* ptr1, const void* ptr2, size_t num)
{
    uint8_t* p1 = (uint8_t*)ptr1;
    uint8_t* p2 = (uint8_t*)ptr2;
    for (size_t i = 0; i < num; i++) {
        int diff = *(p1 + i) - *(p2 + i);
        if (diff != 0) {
            return diff;
        }
    }
    return 0;
}