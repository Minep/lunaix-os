#include <klibc/string.h>

#define WS_CHAR(c)                                                             \
    (c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\v' || c == '\r')

void
strrtrim(char* str)
{
    size_t l = strlen(str);
    while (l < (size_t)-1) {
        char c = str[l];
        if (!c || WS_CHAR(c)) {
            l--;
            continue;
        }
        break;
    }
    str[l + 1] = '\0';
}

char*
strltrim_safe(char* str)
{
    size_t l = 0;
    char c = 0;
    while ((c = str[l]) && WS_CHAR(c)) {
        l++;
    }

    if (!l)
        return str;
    return strcpy(str, str + l);
}