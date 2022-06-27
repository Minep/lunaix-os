#ifndef __LUNAIX_PIC_H
#define __LUNAIX_PIC_H
// FUTURE: Full PIC implementation for fall back when no APIC is detected.

static inline void
pic_disable()
{
    // ref: https://wiki.osdev.org/8259_PIC
    asm volatile("movb $0xff, %al\n"
                 "outb %al, $0xa1\n"
                 "outb %al, $0x21\n");
}

#endif /* __LUNAIX_PIC_H */
