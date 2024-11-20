#include <klibc/string.h>
#include <lunaix/compiler.h>

char* _weak
strcpy(char* dest, const char* src)
{
    char c;
    unsigned int i = 0;
    while ((c = src[i])) {
        dest[i] = c;
        i++;
    }
    dest[i++] = '\0';
    return &dest[i];
}

/**
 * @brief strcpy with constrain on numbers of character.
 *        this version is smarter than stdc, it will automatically
 *        handle the null-terminator.
 * 
 * @param dest 
 * @param src 
 * @param n 
 * @return char* 
 */
char* _weak
strncpy(char* dest, const char* src, unsigned long n)
{
    char c;
    unsigned int i = 0;
    while (i <= n && (c = src[i]))
        dest[i++] = c;

    if (!(n < i && src[i - 1])) {
        while (i <= n)
            dest[i++] = 0;
    }
    else {
        dest[i - 1] = 0;
    }
    
    return dest;
}