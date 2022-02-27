#include <lunaix/assert.h>

void __assert_fail(const char* expr, const char* file, unsigned int line) {
    tty_set_theme(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_RED);
    tty_clear_line(10);
    tty_clear_line(11);
    tty_clear_line(12);
    tty_set_cpos(0, 11);
    printf("  Assert %s failed (%s:%u)", expr, file, line);
    __spin:
        goto __spin;
}