#include <klibc/string.h>

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