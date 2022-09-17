#ifndef __LUNAIX_TTY_H
#define __LUNAIX_TTY_H

#include <lunaix/ds/fifo.h>

typedef unsigned short vga_attribute;

#define VGA_COLOR_BLACK 0
#define VGA_COLOR_BLUE 1
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_CYAN 3
#define VGA_COLOR_RED 4
#define VGA_COLOR_MAGENTA 5
#define VGA_COLOR_BROWN 6
#define VGA_COLOR_DARK_GREY 8
#define VGA_COLOR_LIGHT_GREY 7
#define VGA_COLOR_LIGHT_BLUE 9
#define VGA_COLOR_LIGHT_GREEN 10
#define VGA_COLOR_LIGHT_CYAN 11
#define VGA_COLOR_LIGHT_RED 12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_LIGHT_BROWN 14
#define VGA_COLOR_WHITE 15

#define TTY_WIDTH 80
#define TTY_HEIGHT 25

void
tty_init(void* vga_buf);

void
tty_set_theme(vga_attribute fg, vga_attribute bg);

vga_attribute
tty_get_theme();

void
tty_flush_buffer(struct fifo_buf* buf);

void
tty_clear_line(int line_num);

void
tty_put_str_at(char* str, int x, int y);

#endif /* __LUNAIX_TTY_H */
