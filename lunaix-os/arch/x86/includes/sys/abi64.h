#ifndef __LUNAIX_ARCH_ABI64_H
#define __LUNAIX_ARCH_ABI64_H

#include "sys/x86_isa.h"

#define stack_alignment 0xfffffffffffffff0UL

#ifndef __ASM__
#define store_retval(retval) current_thread->hstate->registers.rax = (retval)

#define store_retval_to(th, retval) (th)->hstate->registers.rax = (retval)

static inline void must_inline 
j_usr(ptr_t sp, ptr_t pc) 
{
    asm volatile(
                 "pushq %0\n"
                 "pushq %1\n"
                 "pushq %2\n"
                 "pushq %3\n"
                 "retfq" 
                 ::
                    "i"(UDATA_SEG),
                    "r"(sp),
                    "i"(UCODE_SEG),
                    "r"(pc)
                 : 
                 "ax", "memory");
}

static inline ptr_t must_inline
abi_get_callframe()
{
    ptr_t val;
    asm("movq %%rbp, %0" : "=r"(val)::);
    return val;
}

#endif
#endif /* __LUNAIX_ARCH_ABI64_H */
