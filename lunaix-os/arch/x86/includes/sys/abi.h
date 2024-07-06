#ifndef __LUNAIX_I386ABI_H
#define __LUNAIX_I386ABI_H

#ifdef CONFIG_ARCH_X86_64
#   include "abi64.h"
#else
#   include "abi32.h"
#endif

#ifndef __ASM__
#define align_stack(ptr) ((ptr) & stack_alignment)

static inline void must_inline noret
switch_context() {
    asm volatile("jmp do_switch\n");
    unreachable;
}


static inline ptr_t
abi_get_retaddr()
{
    return *((ptr_t*)abi_get_callframe() + 1);
}

static inline ptr_t
abi_get_retaddrat(ptr_t fp)
{
    return *((ptr_t*)fp + 1);
}

#endif
#endif /* __LUNAIX_ABI_H */
