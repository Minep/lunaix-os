#ifndef __LUNALIBC_STRING_H
#define __LUNALIBC_STRING_H

#include <stddef.h>

extern size_t strlen(const char* str);

extern size_t strnlen(const char* str, size_t max_len);

extern char* strncpy(char* dest, const char* src, size_t n);

extern const char* strchr(const char* str, int character);

extern char* strcpy(char* dest, const char* src);

extern int strcmp(const char* s1, const char* s2);

#endif /* __LUNALIBC_STRING_H */
