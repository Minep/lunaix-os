#ifndef __LUNAIX_STRING_H
#define __LUNAIX_STRING_H

#include <stddef.h>

int
memcmp(const void* dest, const void* src, size_t size);

void*
memcpy(void* dest, const void* src, size_t size);

void*
memmove(void* dest, const void* src, size_t size);

void*
memset(void* dest, int val, size_t size);

size_t
strlen(const char* str);

char*
strcpy(char* dest, const char* src);

size_t
strnlen(const char* str, size_t max_len);

char*
strncpy(char* dest, const char* src, size_t n);

const char*
strchr(const char* str, int character);

int
streq(const char* a, const char* b);

void
strrtrim(char* str);

char*
strltrim_safe(char* str);

#endif /* __LUNAIX_STRING_H */
