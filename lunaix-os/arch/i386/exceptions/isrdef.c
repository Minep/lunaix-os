/* Generated from i386_isrdef.c.j2. Do NOT modify */

#include <lunaix/types.h>
#include "sys/int_handler.h"
#include "sys/vectors.h"
#include "sys/x86_isa.h"

#define IDT_INTERRUPT 0x70
#define IDT_ATTR(dpl, type) (((type) << 5) | ((dpl & 3) << 13) | (1 << 15))
#define IDT_ENTRY 256

#define DECLARE_ISR(iv) extern void _asm_isr##iv();

#ifndef CONFIG_ARCH_X86_64

static inline void install_idte(u64_t idt, int iv, void* isr, int dpl) 
{
    _idt[iv] = ((ptr_t)isr & 0xffff0000) | IDT_ATTR(dpl, IDT_INTERRUPT); 
    _idt[iv] <<= 32;                                                     
    _idt[iv] |= (KCODE_SEG << 16) | ((ptr_t)isr & 0x0000ffff);           
}
u64_t _idt[IDT_ENTRY];
u16_t _idt_limit = sizeof(_idt) - 1;

#else

struct idte {
    u64_t lo;
    u64_t hi;
} compact;

static inline must_inline void 
install_idte(struct idte* idt, int iv, void* isr, int dpl) 
{
    struct idte* idte = &idt[iv];
    ptr_t isr_p = (ptr_t)isr;

    idte->hi = isr_p >> 32;
    
    idte->lo = (isr_p & 0xffff0000ULL) | IDT_ATTR(dpl, IDT_INTERRUPT); 
    idte->lo |= 1;  // IST=1

    idte->lo <<= 32;                                                     
    idte->lo |= (KCODE_SEG << 16) | (isr_p & 0x0000ffffULL);           
}

struct idte _idt[IDT_ENTRY];
u16_t _idt_limit = sizeof(_idt) - 1;

#endif

DECLARE_ISR(0)
DECLARE_ISR(33)

void
exception_install_handler()
{
    ptr_t start = _asm_isr0;
    
    for (int i = 0; i < 256; i++) {
        install_idte(_idt, i, start, 0);
        start += 16;
    }

    install_idte(_idt, 33, _asm_isr33, 3);

#ifdef CONFIG_ARCH_X86_64
    // TODO set different IST to some exception so it can get a
    //      better and save stack to work with
#endif
}