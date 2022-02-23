#include <libc/string.h>

char*
strcpy(char* dest, const char* src) {
    char c;
    unsigned int i = 0;
    while ((c = src[i]))
    {
        dest[i] = c;
        i++;
    }
    dest[i] = '\0';
    return dest;
}