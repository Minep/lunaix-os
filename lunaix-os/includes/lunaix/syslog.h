#ifndef __LUNAIX_SYSLOG_H
#define __LUNAIX_SYSLOG_H

#include <lunaix/compiler.h>
#include <stdarg.h>

#define KLOG_DEBUG 0
#define KLOG_INFO 1
#define KLOG_WARN 2
#define KLOG_ERROR 3
#define KLOG_FATAL 4

#define _LEVEL_INFO "0"
#define _LEVEL_WARN "1"
#define _LEVEL_ERROR "2"
#define _LEVEL_DEBUG "3"

#define KMSG_LVLSTART '\x1b'
#define KMSG_LOGLEVEL(c) ((c) - '0')

#define KDEBUG "\x1b" stringify__(KLOG_DEBUG)
#define KINFO "\x1b" stringify__(KLOG_INFO)
#define KWARN "\x1b" stringify__(KLOG_WARN)
#define KERROR "\x1b" stringify__(KLOG_ERROR)
#define KFATAL "\x1b" stringify__(KLOG_FATAL)

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

#endif /* __LUNAIX_SYSLOG_H */
