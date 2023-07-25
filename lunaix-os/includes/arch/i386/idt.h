#ifndef __LUNAIX_IDT_H
#define __LUNAIX_IDT_H
#define IDT_TRAP 0x78
#define IDT_INTERRUPT 0x70
#define IDT_ATTR(dpl, type) (((type) << 5) | ((dpl & 3) << 13) | (1 << 15))

void
_init_idt();

#endif /* __LUNAIX_IDT_H */