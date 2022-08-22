#ifndef __LUNAIX_STDIO_H
#define __LUNAIX_STDIO_H
#include <stdarg.h>
#include <stddef.h>

size_t
__ksprintf_internal(char* buffer, char* fmt, size_t max_len, va_list vargs);

size_t
ksprintf(char* buffer, char* fmt, ...);
size_t
ksnprintf(char* buffer, size_t n, char* fmt, ...);
#endif /* __LUNAIX_STDIO_H */
