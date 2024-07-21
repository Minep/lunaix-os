#ifndef __LUNAIX_USTDIO_H
#define __LUNAIX_USTDIO_H

#include <stdarg.h>

#define stdout 0
#define stdin 1

int
printf(const char* fmt, ...);

int
vsnprintf(char* buffer, unsigned int size, const char* fmt, va_list ap);

int
snprintf(char* buffer, unsigned int size, const char* fmt, ...);

#endif /* __LUNAIX_USTDIO_H */
