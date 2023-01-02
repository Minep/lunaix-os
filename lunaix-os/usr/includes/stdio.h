#ifndef __LUNAIX_USTDIO_H
#define __LUNAIX_USTDIO_H

#define stdout 0
#define stdin 1

void
printf(const char* fmt, ...);

const char*
strchr(const char* str, int character);

#endif /* __LUNAIX_USTDIO_H */
