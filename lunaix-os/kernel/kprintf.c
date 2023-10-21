#include <klibc/stdio.h>
#include <lunaix/lxconsole.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>
#include <lunaix/tty/tty.h>

#define MAX_KPRINTF_BUF_SIZE 512
#define MAX_XFMT_SIZE 512

static char* log_prefix[] = { "- ", "W ", "E ", "D " };
static char* color_code[] = { "", "\033[6;0m", "\033[12;0m", "\033[9;0m" };

void 
__kprintf_internal(const char* component,
                   int log_level,
                   const char* prefix,
                   const char* fmt,
                   va_list args)
{
    char buf[MAX_KPRINTF_BUF_SIZE];
    char expanded_fmt[MAX_XFMT_SIZE];
    char* color = color_code[log_level];

    if (component) {
        ksnprintf(expanded_fmt,
                  MAX_XFMT_SIZE,
                  "%s%s%s: %s\033[39;49m",
                  color,
                  prefix,
                  component,
                  fmt);
    } else {
        ksnprintf(
          expanded_fmt, MAX_XFMT_SIZE, "%s%s%s\033[39;49m", color, prefix, fmt);
    }

    __ksprintf_internal(buf, expanded_fmt, MAX_KPRINTF_BUF_SIZE, args);
    console_write_str(buf);
}

char*
__get_loglevel(char* fmt, int* log_level_out)
{
    char l = '0';
    if (*fmt == '\x1b') {
        l = *(++fmt);
        fmt++;
    }
    l -= '0';

    if (l > 3) {
        l = 0;
    }

    *log_level_out = (int)l;
    return fmt;
}

void
kappendf(const char* fmt, ...)
{
    char buf[MAX_KPRINTF_BUF_SIZE];

    va_list args;
    va_start(args, fmt);

    int log_level;
    fmt = __get_loglevel(fmt, &log_level);

    __kprintf_internal(NULL, log_level, "", fmt, args);

    va_end(args);
}

void
__kprintf(const char* component, const char* fmt, va_list args)
{
    if (!fmt)
        return;

    int log_level;
    fmt = __get_loglevel(fmt, &log_level);
    __kprintf_internal(component, log_level, log_prefix[log_level], fmt, args);
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

    spin();
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
    __kprintf_internal("syslog", level, "", fmt, args);
}