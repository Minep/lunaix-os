#ifndef __LUNAIX__MYSTDIO_H
#define __LUNAIX__MYSTDIO_H

#include <stdarg.h>
#include <stddef.h>

int
__vprintf_internal(char* buffer,
                   const char* fmt,
                   size_t max_len,
                   va_list vargs);

#endif /* __LUNAIX__MYSTDIO_H */
