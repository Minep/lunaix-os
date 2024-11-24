#include <lunaix/spike.h>
#include <stdio.h>
#include <unistd.h>

void
__assert_fail(const char* expr, const char* file, unsigned int line)
{
    printf("assertion failed: %s (%s:%d)\n", expr, file, line);

    _exit(1);
}