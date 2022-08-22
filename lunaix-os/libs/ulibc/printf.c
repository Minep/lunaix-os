#include <klibc/stdio.h>
#include <lunaix/lunistd.h>
#include <lunaix/spike.h>
#include <ulibc/stdio.h>

// This is VERY bad implementation as it mixes both kernel and user space
// code together. It is here however, just for the convenience of our testing
// program.
// FIXME Eliminate this when we're able to load program.

void __USER__
printf(const char* fmt, ...)
{
    const char buf[512];
    va_list args;
    va_start(args, fmt);

    size_t sz = __ksprintf_internal(buf, fmt, 512, args);

    write(stdout, buf, sz);

    va_end(args);
}