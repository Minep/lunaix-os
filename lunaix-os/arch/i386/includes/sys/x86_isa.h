#ifndef __LUNAIX_I386_ASM_H
#define __LUNAIX_I386_ASM_H

#define SEL_INDEX(i)    ((i) << 3)
#define SEL_RPL(rpl)    ((rpl) & 0b11)

#define KCODE_SEG (SEL_INDEX(1) | SEL_RPL(0))
#define UCODE_SEG (SEL_INDEX(2) | SEL_RPL(3))

#ifdef CONFIG_ARCH_I386
    #define KDATA_SEG (SEL_INDEX(2) | SEL_RPL(0))
    #define UDATA_SEG (SEL_INDEX(4) | SEL_RPL(3))
    #define TSS_SEG   (SEL_INDEX(5) | SEL_RPL(0))

#else
    #define KDATA_SEG (SEL_INDEX(3) | SEL_RPL(0))
    #define UDATA_SEG (SEL_INDEX(3) | SEL_RPL(3))
    #define TSS_SEG   (SEL_INDEX(4) | SEL_RPL(0))

#endif


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

#ifdef CONFIG_ARCH_X86_64

struct x86_tss
{
    u32_t rsvd_1;
    ptr_t rsps[3];
    union {
        struct {
            ptr_t ist_null;
            ptr_t valid_ists[7];
        };
        ptr_t ists[8];
    };
    
    u8_t rsvd_3[12];
} compact;

typedef u64_t x86_segdesc_t;

struct x86_sysdesc {
    u64_t hi;
    u64_t lo;
} compact;

#else

struct x86_tss
{
    u32_t link;
    u32_t esp0;
    u16_t ss0;
    u8_t __padding[94];
} compact;

typedef struct {
    u32_t lo;
    u32_t hi;
} compact x86_segdesc_t;

#endif

#endif
#endif /* __LUNAIX_I386_ASM_H */
