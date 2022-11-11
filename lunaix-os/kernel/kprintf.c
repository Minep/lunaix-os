#include <klibc/stdio.h>
#include <lunaix/lxconsole.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>
#include <lunaix/tty/tty.h>

#define MAX_KPRINTF_BUF_SIZE 512
#define MAX_XFMT_SIZE 512

void
__kprintf_internal(const char* component,
                   int log_level,
                   const char* fmt,
                   va_list args)
{
    char buf[MAX_KPRINTF_BUF_SIZE];
    char expanded_fmt[MAX_XFMT_SIZE];

    switch (log_level) {
        case 1:
            // tty_set_theme(VGA_COLOR_BROWN, current_theme >> 12);
            ksnprintf(expanded_fmt,
                      MAX_XFMT_SIZE,
                      "\033[6;0mW %s: %s\033[39;49m",
                      component,
                      fmt);
            break;
        case 2:
            // tty_set_theme(VGA_COLOR_LIGHT_RED, current_theme >> 12);
            ksnprintf(expanded_fmt,
                      MAX_XFMT_SIZE,
                      "\033[12;0mE %s: %s\033[39;49m",
                      component,
                      fmt);
            break;
        case 3:
            // tty_set_theme(VGA_COLOR_LIGHT_BLUE, current_theme >> 12);
            ksnprintf(expanded_fmt,
                      MAX_XFMT_SIZE,
                      "\033[9;0mD %s: %s\033[39;49m",
                      component,
                      fmt);
            break;
        default:
            ksnprintf(expanded_fmt, MAX_XFMT_SIZE, "- %s: %s", component, fmt);
            break;
    }

    __ksprintf_internal(buf, expanded_fmt, MAX_KPRINTF_BUF_SIZE, args);
    console_write_str(buf);
}

void
__kprintf(const char* component, const char* fmt, va_list args)
{
    if (!fmt)
        return;
    char log_level = '0';

    if (*fmt == '\x1b') {
        log_level = *(++fmt);
        fmt++;
    }

    __kprintf_internal(component, log_level - '0', fmt, args);
}

void
kprint_panic(const char* fmt, ...)
{
    char buf[MAX_KPRINTF_BUF_SIZE];
    va_list args;
    va_start(args, fmt);

    tty_set_theme(VGA_COLOR_WHITE, VGA_COLOR_RED);
    tty_clear_line(24);

    __ksprintf_internal(buf, fmt, MAX_KPRINTF_BUF_SIZE, args);
    tty_put_str_at(buf, 0, 24);

    va_end(args);
}

void
kprint_dbg(const char* fmt, ...)
{
    char buf[MAX_KPRINTF_BUF_SIZE];
    va_list args;
    va_start(args, fmt);

    tty_set_theme(VGA_COLOR_WHITE, VGA_COLOR_MAGENTA);
    tty_clear_line(24);

    __ksprintf_internal(buf, fmt, MAX_KPRINTF_BUF_SIZE, args);
    tty_put_str_at(buf, 0, 24);

    va_end(args);

    tty_set_theme(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

void
kprint_hex(const void* buffer, unsigned int size)
{
    unsigned char* data = (unsigned char*)buffer;
    char buf[16];
    char ch_cache[16];
    unsigned int ptr = 0;
    int i;

    ch_cache[0] = '|';
    ch_cache[1] = ' ';
    while (size) {
        ksnprintf(buf, 64, " %.4p: ", ptr);
        console_write_str(buf);
        for (i = 0; i < 8 && size; i++, size--, ptr++) {
            unsigned char c = *(data + ptr) & 0xff;
            ch_cache[2 + i] = (32 <= c && c < 127) ? c : '.';
            ksnprintf(buf, 64, "%.2x  ", c);
            console_write_str(buf);
        }
        ch_cache[2 + i] = '\0';
        console_write_str(ch_cache);
        console_write_char('\n');
    }
}

__DEFINE_LXSYSCALL3(void, syslog, int, level, const char*, fmt, va_list, args)
{
    __kprintf_internal("syslog", level, fmt, args);
}