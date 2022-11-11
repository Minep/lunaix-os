#ifndef __LUNAIX_LXCONSOLE_H
#define __LUNAIX_LXCONSOLE_H

#define TCINTR 0x03 // ETX
#define TCEOF 0x04  // EOT
#define TCBS 0x08   // BS
#define TCLF 0x0A   // LF
#define TCCR 0x0D   // CR
#define TCSTOP 0x1A // SUB

void
lxconsole_init();

void
lxconsole_spawn_ttydev();

void
console_write_str(char* str);

void
console_write_char(char chr);

void
console_view_up();

void
console_view_down();

void
console_flush();

void
console_start_flushing();
#endif /* __LUNAIX_LXCONSOLE_H */
