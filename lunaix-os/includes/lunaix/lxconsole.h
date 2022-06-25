#ifndef __LUNAIX_LXCONSOLE_H
#define __LUNAIX_LXCONSOLE_H

void
lxconsole_init();

void
console_write_str(char* str);

void
console_write_char(char chr);

void
console_view_up();

void
console_view_down();

void
console_start_flushing();
#endif /* __LUNAIX_LXCONSOLE_H */
