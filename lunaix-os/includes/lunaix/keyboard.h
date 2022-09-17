#ifndef __LUNAIX_KEYBOARD_H
#define __LUNAIX_KEYBOARD_H
#include <lunaix/clock.h>

//      Lunaix Keycode
//       15        7         0
// key = |xxxx xxxx|xxxx xxxx|
// key[0:7] = sequence
// key[8:15] = category
//   0x0: ASCII codes
//   0x1: keypad keys
//   0x2: Function keys
//   0x3: Cursor keys (arrow keys)
//   0x4: Modifier keys
//   0xff: Other keys (Un-categorized)

typedef unsigned short kbd_keycode_t;
typedef unsigned short kbd_kstate_t;

#define KEYPAD 0x0100
#define FN_KEY 0x0200
#define CURSOR 0x0300
#define MODIFR 0x0400
#define OTHERS 0xff00

#define ON_KEYPAD(x) ((x & 0xff) | KEYPAD)

// backspace key
#define KEY_BS (0x08)

// enter/return key
#define KEY_LF (0x0a)

#define KEY_HTAB (0x9)
#define KEY_SPACE (0x20)
#define KEY_ESC (0x1b)

#define KEY_F1 (0x00 | FN_KEY)
#define KEY_F2 (0x01 | FN_KEY)
#define KEY_F3 (0x02 | FN_KEY)
#define KEY_F4 (0x03 | FN_KEY)
#define KEY_F5 (0x04 | FN_KEY)
#define KEY_F6 (0x05 | FN_KEY)
#define KEY_F7 (0x06 | FN_KEY)
#define KEY_F8 (0x07 | FN_KEY)
#define KEY_F9 (0x08 | FN_KEY)
#define KEY_F10 (0x09 | FN_KEY)
#define KEY_F11 (0x0a | FN_KEY)
#define KEY_F12 (0x0b | FN_KEY)
#define KEY_CAPSLK (0x0c | FN_KEY)
#define KEY_NUMSLK (0x0d | FN_KEY)
#define KEY_SCRLLK (0x0e | FN_KEY)

#define KEY_PG_UP (0x0 | OTHERS)
#define KEY_PG_DOWN (0x1 | OTHERS)
#define KEY_INSERT (0x2 | OTHERS)
#define KEY_DELETE (0x3 | OTHERS)
#define KEY_HOME (0x4 | OTHERS)
#define KEY_END (0x5 | OTHERS)
#define KEY_PAUSE (0x6 | OTHERS)

#define KEY_LEFT (0x0 | CURSOR)
#define KEY_RIGHT (0x1 | CURSOR)
#define KEY_UP (0x2 | CURSOR)
#define KEY_DOWN (0x3 | CURSOR)

#define KEY_LSHIFT (0x0 | MODIFR)
#define KEY_RSHIFT (0x1 | MODIFR)
#define KEY_LCTRL (0x2 | MODIFR)
#define KEY_RCTRL (0x3 | MODIFR)
#define KEY_LALT (0x4 | MODIFR)
#define KEY_RALT (0x5 | MODIFR)

#define KBD_KEY_FRELEASED 0x0
#define KBD_KEY_FPRESSED 0x1
#define KBD_KEY_FSCRLLKED 0x2
#define KBD_KEY_FNUMBLKED 0x4
#define KBD_KEY_FCAPSLKED 0x8

#define KBD_KEY_FLSHIFT_HELD 0x10
#define KBD_KEY_FRSHIFT_HELD 0x20
#define KBD_KEY_FLCTRL_HELD 0x40
#define KBD_KEY_FRCTRL_HELD 0x80
#define KBD_KEY_FLALT_HELD 0x100
#define KBD_KEY_FRALT_HELD 0x200

#endif /* __LUNAIX_KEYBOARD_H */
