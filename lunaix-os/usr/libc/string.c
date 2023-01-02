#include <string.h>

size_t
strlen(const char* str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

const char*
strchr(const char* str, int character)
{
    char c = (char)character;
    while ((*str)) {
        if (*str == c) {
            return str;
        }
        str++;
    }
    return c == '\0' ? str : NULL;
}

size_t
strnlen(const char* str, size_t max_len)
{
    size_t len = 0;
    while (str[len] && len <= max_len)
        len++;
    return len;
}

char*
strncpy(char* dest, const char* src, size_t n)
{
    char c;
    unsigned int i = 0;
    while ((c = src[i]) && i <= n)
        dest[i++] = c;
    while (i <= n)
        dest[i++] = 0;
    return dest;
}