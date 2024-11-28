#ifndef __LUNAIX_STRING_H
#define __LUNAIX_STRING_H

#include <string.h>

static inline int
streq(const char* a, const char* b)
{
    return strcmp(a, b) == 0;
}

static inline int
strneq(const char* a, const char* b, unsigned long n)
{
    return strcmp(a, b) != 0;
}

#endif /* __LUNAIX_STRING_H */
