#include <klibc/stdio.h>
#include <lunaix/lxconsole.h>
#include <lunaix/syslog.h>
#include <lunaix/tty/tty.h>

#define MAX_KPRINTF_BUF_SIZE 512
#define MAX_XFMT_SIZE 512

void
__kprintf(const char* component, const char* fmt, va_list args)
{
    char buf[MAX_KPRINTF_BUF_SIZE];
    if (!fmt)
        return;
    char log_level = '0';
    char expanded_fmt[MAX_XFMT_SIZE];

    if (*fmt == '\x1b') {
        log_level = *(++fmt);
        fmt++;
    }

    switch (log_level) {
        case '0':
            snprintf(expanded_fmt,
                     MAX_XFMT_SIZE,
                     "[%s] (%s) %s",
                     "INFO",
                     component,
                     fmt);
            break;
        case '1':
            // tty_set_theme(VGA_COLOR_BROWN, current_theme >> 12);
            snprintf(expanded_fmt,
                     MAX_XFMT_SIZE,
                     "\x033[6;0m[%s] (%s) %s\x033[39;49m",
                     "WARN",
                     component,
                     fmt);
            break;
        case '2':
            // tty_set_theme(VGA_COLOR_LIGHT_RED, current_theme >> 12);
            snprintf(expanded_fmt,
                     MAX_XFMT_SIZE,
                     "\x033[12;0m[%s] (%s) %s\x033[39;49m",
                     "EROR",
                     component,
                     fmt);
            break;
        case '3':
            // tty_set_theme(VGA_COLOR_LIGHT_BLUE, current_theme >> 12);
            snprintf(expanded_fmt,
                     MAX_XFMT_SIZE,
                     "\x033[9;0m[%s] (%s) %s\x033[39;49m",
                     "DEBG",
                     component,
                     fmt);
            break;
        default:
            snprintf(expanded_fmt,
                     MAX_XFMT_SIZE,
                     "[%s] (%s) %s",
                     "LOG",
                     component,
                     fmt);
            break;
    }

    __sprintf_internal(buf, expanded_fmt, MAX_KPRINTF_BUF_SIZE, args);
    console_write_str(buf);
}

void
kprint_panic(const char* fmt, ...)
{
    char buf[MAX_KPRINTF_BUF_SIZE];
    va_list args;
    va_start(args, fmt);

    tty_set_theme(VGA_COLOR_WHITE, VGA_COLOR_RED);
    tty_clear_line(24);

    __sprintf_internal(buf, fmt, MAX_KPRINTF_BUF_SIZE, args);
    tty_put_str_at(buf, 0, 24);

    va_end(args);
}