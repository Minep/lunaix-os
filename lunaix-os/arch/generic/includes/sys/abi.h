#ifndef __LUNAIX_ARCH_ABI_H
#define __LUNAIX_ARCH_ABI_H

#include <lunaix/types.h>

#define stack_alignment 0

#ifndef __ASM__
#define align_stack(ptr) ((ptr) & stack_alignment)
#define store_retval(retval) (void)

#define store_retval_to(th, retval) (void)

#define eret_target(th) (void)
#define eret_stack(th)  (void)
#define intr_ivec(th)   (void)
#define intr_ierr(th)   (void)

static inline void must_inline noret
j_usr(ptr_t sp, ptr_t pc) 
{

}


static inline void must_inline noret
switch_context() {
    unreachable;
}

#define push_arg1(stack_ptr, arg) (void)

#define push_arg2(stack_ptr, arg1, arg2)                                       \
    { }

#define push_arg3(stack_ptr, arg1, arg2, arg3)                                 \
    { }

#define push_arg4(stack_ptr, arg1, arg2, arg3, arg4)                           \
    { }


static inline ptr_t must_inline
abi_get_callframe()
{
    return 0;
}

static inline ptr_t
abi_get_retaddr()
{
    return 0;
}

static inline ptr_t
abi_get_retaddrat(ptr_t fp)
{
    return 0;
}

#endif
#endif /* __LUNAIX_ABI_H */

