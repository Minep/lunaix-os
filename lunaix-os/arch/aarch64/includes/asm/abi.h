#ifndef __LUNAIX_AA64_ABI_H
#define __LUNAIX_AA64_ABI_H

#include <lunaix/types.h>

#ifndef __ASM__
#define align_stack(ptr) ((ptr) & ~15)

static inline void must_inline noret
switch_context() {
    // TODO
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

#endif

#endif /* __LUNAIX_AA64_ABI_H */
