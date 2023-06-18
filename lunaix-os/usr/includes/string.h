#ifndef __LUNAIX_STRING_H
#define __LUNAIX_STRING_H

#include <stddef.h>

size_t
strlen(const char* str);

size_t
strnlen(const char* str, size_t max_len);

char*
strncpy(char* dest, const char* src, size_t n);

const char*
strchr(const char* str, int character);

char*
strcpy(char* dest, const char* src);

int
strcmp(const char* s1, const char* s2);

#endif /* __LUNAIX_STRING_H */
