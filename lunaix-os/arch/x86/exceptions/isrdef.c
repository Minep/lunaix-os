/* Generated from i386_isrdef.c.j2. Do NOT modify */

#include <lunaix/types.h>
#include "sys/int_handler.h"
#include "sys/vectors.h"
#include "sys/x86_isa.h"

#define IDT_INTERRUPT 0x70
#define IDT_ATTR(dpl, type) (((type) << 5) | ((dpl & 3) << 13) | (1 << 15))
#define IDT_ENTRY 256

#define __expand_iv(iv) _asm_isr##iv()
#define DECLARE_ISR(iv) extern void __expand_iv(iv);

#ifndef CONFIG_ARCH_X86_64

static inline void 
install_idte(struct x86_sysdesc* idt, int iv, ptr_t isr, int dpl) 
{
    struct x86_sysdesc* idte = &idt[iv];

    idte->hi = ((ptr_t)isr & 0xffff0000) | IDT_ATTR(dpl, IDT_INTERRUPT); 
    idte->lo = (KCODE_SEG << 16) | ((ptr_t)isr & 0x0000ffff);           
}
struct x86_sysdesc _idt[IDT_ENTRY];
u16_t _idt_limit = sizeof(_idt) - 1;

#else

static inline must_inline void 
install_idte(struct x86_sysdesc* idt, int iv, ptr_t isr, int dpl) 
{
    struct x86_sysdesc* idte = &idt[iv];
    
    idte->hi = isr >> 32;
    
    idte->lo = (isr & 0xffff0000UL) | IDT_ATTR(dpl, IDT_INTERRUPT); 

    idte->lo <<= 32;                                                     
    idte->lo |= (KCODE_SEG << 16) | (isr & 0x0000ffffUL);
}

struct x86_sysdesc _idt[IDT_ENTRY];
u16_t _idt_limit = sizeof(_idt) - 1;

#endif

DECLARE_ISR(0)
DECLARE_ISR(33)

void
exception_install_handler()
{
    ptr_t start = (ptr_t)_asm_isr0;
    
    for (int i = 0; i < 256; i++) {
        install_idte(_idt, i, start, 0);
        start += 16;
    }

    install_idte(_idt, 33, (ptr_t)_asm_isr33, 3);

#ifdef CONFIG_ARCH_X86_64
    // TODO set different IST to some exception so it can get a
    //      better and safe stack to work with
#endif
}