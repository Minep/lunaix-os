#ifndef __LUNALIBC_USTDIO_H
#define __LUNALIBC_USTDIO_H

#include <stdarg.h>

#define stdout 0
#define stdin 1

int
printf(const char* fmt, ...);

int
vsnprintf(char* buffer, unsigned int size, const char* fmt, va_list ap);

int
snprintf(char* buffer, unsigned int size, const char* fmt, ...);

#endif /* __LUNALIBC_USTDIO_H */
