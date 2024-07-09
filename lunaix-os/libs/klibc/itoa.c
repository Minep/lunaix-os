#define __LUNAIX_LIBC
#include <klibc/ia_utils.h>
#include <lunaix/types.h>

char base_char[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static char*
__uitoa_internal(unsigned long value, char* str, int base, unsigned int* size)
{
    unsigned int ptr = 0;
    do {
        str[ptr] = base_char[value % base];
        value = value / (unsigned long)base;
        ptr++;
    } while (value);

    for (unsigned int i = 0; i < (ptr >> 1); i++) {
        char c = str[i];
        str[i] = str[ptr - i - 1];
        str[ptr - i - 1] = c;
    }
    str[ptr] = '\0';
    if (size) {
        *size = ptr;
    }
    return str;
}

static char*
__itoa_internal(long value, char* str, int base, unsigned int* size)
{
    if (value < 0 && base == 10) {
        str[0] = '-';
        unsigned long _v = (unsigned long)(-value);
        __uitoa_internal(_v, str + 1, base, size);
    } else {
        __uitoa_internal(value, str, base, size);
    }

    return str;
}

char*
itoa(long value, char* str, int base)
{
    return __itoa_internal(value, str, base, NULL);
}