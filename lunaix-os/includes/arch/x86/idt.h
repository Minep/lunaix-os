#ifndef __LUNAIX_IDT_H
#define __LUNAIX_IDT_H 1
#define IDT_ATTR(dpl)                   ((0x70 << 5) | (dpl & 3) << 13 | 1 << 15)

void
_init_idt();
#endif