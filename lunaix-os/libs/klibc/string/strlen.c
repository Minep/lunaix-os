#include <klibc/string.h>
#include <lunaix/compiler.h>

unsigned long _weak
strlen(const char *str)
{
    unsigned long len = 0;
    while (str[len])
        len++;
    return len;
}

unsigned long _weak
strnlen(const char *str, unsigned long max_len)
{
    unsigned long len = 0;
    while (str[len] && len <= max_len)
        len++;
    return len;
}