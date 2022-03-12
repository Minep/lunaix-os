#include <klibc/string.h>

size_t
strlen(const char* str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

size_t
strnlen(const char* str, size_t max_len) {
    size_t len = 0;
    while (str[len] && len <= max_len)
        len++;
    return len;
}