#include <klibc/string.h>
#include <lunaix/compiler.h>

int weak
streq(const char* a, const char* b)
{
    while (*a == *b) {
        if (!(*a)) {
            return 1;
        }
        a++;
        b++;
    }
    return 0;
}

int
strneq(const char* a, const char* b, unsigned long n)
{
    while (n-- && *a == *b) {
        if (!(*a)) {
            return 1;
        }

        a++;
        b++;
    }
    return !(n + 1);
}