#include <lunaix/syslog.h>
#include <lunaix/tty/tty.h>
#include <klibc/stdio.h>

#define MAX_KPRINTF_BUF_SIZE 1024
#define MAX_XFMT_SIZE 1024

char buf[MAX_KPRINTF_BUF_SIZE];

void
__kprintf(const char* component, const char* fmt, va_list args) {
    if (!fmt) return;
    char log_level = '0';
    char expanded_fmt[MAX_XFMT_SIZE];
    vga_attribute current_theme = tty_get_theme();

    if (*fmt == '\x1b') {
        log_level = *(++fmt);
        fmt++;
    }

    switch (log_level)
    {
    case '0':
        snprintf(expanded_fmt, MAX_XFMT_SIZE, "[%s] (%s) %s", "INFO", component, fmt);
        break;
    case '1':
        tty_set_theme(VGA_COLOR_BROWN, current_theme >> 12);
        snprintf(expanded_fmt, MAX_XFMT_SIZE, "[%s] (%s) %s", "INFO", component, fmt);
        break;
    case '2':
        tty_set_theme(VGA_COLOR_LIGHT_RED, current_theme >> 12);
        snprintf(expanded_fmt, MAX_XFMT_SIZE, "[%s] (%s) %s", "EROR", component, fmt);
        break;
    default:
        snprintf(expanded_fmt, MAX_XFMT_SIZE, "[%s] (%s) %s", "LOG", component, fmt);
        break;
    }

    __sprintf_internal(buf, expanded_fmt, MAX_KPRINTF_BUF_SIZE, args);
    tty_put_str(buf);
    tty_set_theme(current_theme >> 8, current_theme >> 12);
}

void
kprint_panic(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    tty_set_theme(VGA_COLOR_WHITE, VGA_COLOR_RED);
    tty_clear_line(10);
    tty_clear_line(11);
    tty_clear_line(12);
    tty_set_cpos(0, 11);

    __sprintf_internal(buf, fmt, MAX_KPRINTF_BUF_SIZE, args);
    tty_put_str(buf);

    va_end(args);
}