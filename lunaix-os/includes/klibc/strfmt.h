#ifndef __LUNAIX_STRFMT_H
#define __LUNAIX_STRFMT_H
#include <stdarg.h>

unsigned long
ksnprintfv(char* buffer, const char* fmt, unsigned long max_len, va_list vargs);

unsigned long
ksprintf(char* buffer, char* fmt, ...);
unsigned long
ksnprintf(char* buffer, unsigned long n, char* fmt, ...);
#endif /* __LUNAIX_STRFMT_H */
