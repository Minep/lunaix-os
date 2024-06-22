#ifndef __LUNAIX_I386_ASM_H
#define __LUNAIX_I386_ASM_H

#define KCODE_SEG 0x08
#define KDATA_SEG 0x10
#define UCODE_SEG 0x1B
#define UDATA_SEG 0x23
#define TSS_SEG 0x28

#define tss_esp0_off 4

#ifndef __ASM__
#include <lunaix/types.h>

#define IRQ_TRIG_EDGE 0b0
#define IRQ_TRIG_LEVEL 0b1

#define IRQ_TYPE_FIXED (0b01 << 1)
#define IRQ_TYPE_NMI (0b11 << 1)
#define IRQ_TYPE (0b11 << 1)

#define IRQ_VE_HI (0b1 << 3)
#define IRQ_VE_LO (0b0 << 3)

#define IRQ_DEFAULT (IRQ_TRIG_EDGE | IRQ_TYPE_FIXED | IRQ_VE_HI)


struct x86_tss
{
  u32_t link;
  u32_t esp0;
  u16_t ss0;
  u8_t __padding[94];
} __attribute__((packed));

void tss_update_esp(u32_t esp0);

struct x86_intc
{
    char* name;
    void* data;

    void (*irq_attach)(struct x86_intc*,
                       int irq,
                       int iv,
                       cpu_t dest,
                       u32_t flags);
    void (*notify_eoi)(struct x86_intc*, cpu_t id, int iv);
};


#endif

#endif /* __LUNAIX_I386_ASM_H */
