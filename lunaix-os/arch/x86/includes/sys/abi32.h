#ifndef __LUNAIX_ARCH_ABI32_H
#define __LUNAIX_ARCH_ABI32_H

#include "sys/x86_isa.h"

#define stack_alignment 0xfffffff0

#ifndef __ASM__
#define store_retval(retval) current_thread->hstate->registers.eax = (retval)

#define store_retval_to(th, retval) (th)->hstate->registers.eax = (retval)

static inline void must_inline 
j_usr(ptr_t sp, ptr_t pc) 
{
    asm volatile("movw %0, %%ax\n"
                 "movw %%ax, %%es\n"
                 "movw %%ax, %%ds\n"
                 "movw %%ax, %%fs\n"
                 "movw %%ax, %%gs\n"
                 "pushl %0\n"
                 "pushl %1\n"
                 "pushl %2\n"
                 "pushl %3\n"
                 "retf" ::"i"(UDATA_SEG),
                 "r"(sp),
                 "i"(UCODE_SEG),
                 "r"(pc)
                 : "eax", "memory");
}

static inline ptr_t must_inline
abi_get_callframe()
{
    ptr_t val;
    asm("movl %%ebp, %0" : "=r"(val)::);
    return val;
}

#endif
#endif /* __LUNAIX_ARCH_ABI32_H */
