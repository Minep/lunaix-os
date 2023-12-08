#ifndef __LUNAIX_STRING_H
#define __LUNAIX_STRING_H

int
memcmp(const void* dest, const void* src, unsigned long size);

void*
memcpy(void* dest, const void* src, unsigned long size);

void*
memmove(void* dest, const void* src, unsigned long size);

void*
memset(void* dest, int val, unsigned long size);

unsigned long
strlen(const char* str);

char*
strcpy(char* dest, const char* src);

unsigned long
strnlen(const char* str, unsigned long max_len);

char*
strncpy(char* dest, const char* src, unsigned long n);

const char*
strchr(const char* str, int character);

int
streq(const char* a, const char* b);

void
strrtrim(char* str);

char*
strltrim_safe(char* str);

#endif /* __LUNAIX_STRING_H */
