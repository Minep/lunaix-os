#ifndef __LUNAIX_SYSLOG_H
#define __LUNAIX_SYSLOG_H

#include <lunaix/compiler.h>
#include <lunaix/spike.h>
#include <stdarg.h>

#define KLOG_DEBUG 0
#define KLOG_INFO 1
#define KLOG_WARN 2
#define KLOG_ERROR 3
#define KLOG_FATAL 4

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
        kprintf_m(module, fmt, args);                                          \
        va_end(args);                                                          \
    }

#define DEBUG(fmt, ...) kprintf(KDEBUG fmt, ##__VA_ARGS__)
#define INFO(fmt, ...) kprintf(KINFO fmt, ##__VA_ARGS__)
#define WARN(fmt, ...) kprintf(KWARN fmt, ##__VA_ARGS__)
#define ERROR(fmt, ...) kprintf(KERROR fmt, ##__VA_ARGS__)
#define FATAL(fmt, ...)                                                        \
    ({                                                                         \
        kprintf(KFATAL fmt, ##__VA_ARGS__);                                    \
        fail("critical failure");                                              \
    })

void
kprintf_m(const char* component, const char* fmt, va_list args);
#endif /* __LUNAIX_SYSLOG_H */
