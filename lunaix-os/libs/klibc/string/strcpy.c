#include <klibc/string.h>

char*
strcpy(char* dest, const char* src)
{
    char c;
    unsigned int i = 0;
    while ((c = src[i])) {
        dest[i] = c;
        i++;
    }
    dest[i] = '\0';
    return dest;
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