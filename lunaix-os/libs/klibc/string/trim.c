#include <klibc/string.h>
#include <lunaix/compiler.h>

#define WS_CHAR(c)                                                             \
    (c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\v' || c == '\r')

void _weak
strrtrim(char* str)
{
    unsigned long l = strlen(str);
    while (l < (unsigned long)-1) {
        char c = str[l];
        if (!c || WS_CHAR(c)) {
            l--;
            continue;
        }
        break;
    }
    str[l + 1] = '\0';
}

char* _weak
strltrim_safe(char* str)
{
    unsigned long l = 0;
    char c = 0;
    while ((c = str[l]) && WS_CHAR(c)) {
        l++;
    }

    if (l)
        strcpy(str, str + l);
    return str;
}