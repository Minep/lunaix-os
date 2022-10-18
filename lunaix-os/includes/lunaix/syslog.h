#ifndef __LUNAIX_SYSLOG_H
#define __LUNAIX_SYSLOG_H

#include <stdarg.h>

#define _LEVEL_INFO "0"
#define _LEVEL_WARN "1"
#define _LEVEL_ERROR "2"
#define _LEVEL_DEBUG "3"

#define KINFO "\x1b" _LEVEL_INFO
#define KWARN "\x1b" _LEVEL_WARN
#define KERROR "\x1b" _LEVEL_ERROR
#define KDEBUG "\x1b" _LEVEL_DEBUG

#define LOG_MODULE(module)                                                     \
    static void kprintf(const char* fmt, ...)                                  \
    {                                                                          \
        va_list args;                                                          \
        va_start(args, fmt);                                                   \
        __kprintf(module, fmt, args);                                          \
        va_end(args);                                                          \
    }

void
__kprintf(const char* component, const char* fmt, va_list args);

void
kprint_hex(const void* buffer, unsigned int size);

void
kprint_panic(const char* fmt, ...);

void
kprint_dbg(const char* fmt, ...);

#endif /* __LUNAIX_SYSLOG_H */
