#include <klibc/string.h>

int
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