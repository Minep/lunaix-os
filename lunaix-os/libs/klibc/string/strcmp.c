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