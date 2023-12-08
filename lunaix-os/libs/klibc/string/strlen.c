#include <klibc/string.h>

unsigned long
strlen(const char* str)
{
    unsigned long len = 0;
    while (str[len])
        len++;
    return len;
}

unsigned long
strnlen(const char* str, unsigned long max_len)
{
    unsigned long len = 0;
    while (str[len] && len <= max_len)
        len++;
    return len;
}