#include <lunaix/syslog.h>
#include <stdio.h>
#include <stdlib.h>

void
kprintf_m(const char* component, const char* fmt, va_list args)
{
    int sz = 0;
    char buff[1024];
    
    sz = vsnprintf(buff, 1024, fmt, args);
    printf("%s: %s\n", component, buff);
}

void
kprintf_v(const char* component, const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    kprintf_m(component, fmt, args);

    va_end(args);
}