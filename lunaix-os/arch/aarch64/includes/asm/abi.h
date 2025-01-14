#ifndef __LUNAIX_AA64_ABI_H
#define __LUNAIX_AA64_ABI_H

#include <lunaix/types.h>

#ifndef __ASM__

#include <asm/aa64_msrs.h>
#include <asm/aa64_spsr.h>

#define align_stack(ptr) ((ptr) & ~15)

#define store_retval(retval) \
            current_thread->hstate->registers.x[0] = (retval)

#define store_retval_to(th, retval) \
            (th)->hstate->registers.x[0] = (retval)


static inline void must_inline noret
switch_context() 
{
    asm ("b _aa64_switch_task");
    unreachable;
}


static inline ptr_t
abi_get_retaddr()
{
    reg_t lr;
    asm ("mov %0, lr" : "=r"(lr));

    return lr;
}

static inline ptr_t
abi_get_retaddrat(ptr_t fp)
{
    return ((ptr_t*)fp)[1];
}

static inline ptr_t must_inline
abi_get_callframe()
{
    ptr_t val;
    asm volatile("mov %0, fp" : "=r"(val));
    return val;
}

static inline void must_inline 
j_usr(ptr_t sp, ptr_t pc) 
{
    set_sysreg(SPSR_EL1, SPSR_EL0_preset);
    set_sysreg(SP_EL0, sp);
    set_sysreg(ELR_E1, pc);
    asm ("eret");

    unreachable;
}

#endif

#endif /* __LUNAIX_AA64_ABI_H */
