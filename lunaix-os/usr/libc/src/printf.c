#include "_mystdio.h"
#include <stdio.h>
#include <unistd.h>

int
printf(const char* fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int n = __vprintf_internal(buf, fmt, 512, args);
    va_end(args);

    return write(stdout, buf, n);
}