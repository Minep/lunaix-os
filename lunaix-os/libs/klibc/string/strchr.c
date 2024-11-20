#include <klibc/string.h>
#include <lunaix/types.h>

const char* _weak
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