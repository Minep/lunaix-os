#ifndef __LUNAIX_I386ABI_H
#define __LUNAIX_I386ABI_H

#include <sys/x86_isa.h>

#define stack_alignment 0xfffffff0

#ifndef __ASM__
#define align_stack(ptr) ((ptr) & stack_alignment)
#define store_retval(retval) current_thread->intr_ctx->registers.eax = (retval)

#define store_retval_to(th, retval) (th)->intr_ctx->registers.eax = (retval)

#define eret_target(th) (th)->intr_ctx->execp->eip
#define eret_stack(th) (th)->intr_ctx->execp->esp
#define intr_ivec(th) (th)->intr_ctx->execp->vector
#define intr_ierr(th) (th)->intr_ctx->execp->err_code

#define j_usr(sp, pc)                                                          \
    asm volatile("movw %0, %%ax\n"                                             \
                 "movw %%ax, %%es\n"                                           \
                 "movw %%ax, %%ds\n"                                           \
                 "movw %%ax, %%fs\n"                                           \
                 "movw %%ax, %%gs\n"                                           \
                 "pushl %0\n"                                                  \
                 "pushl %1\n"                                                  \
                 "pushl %2\n"                                                  \
                 "pushl %3\n"                                                  \
                 "retf" ::"i"(UDATA_SEG),                                      \
                 "r"(sp),                                                      \
                 "i"(UCODE_SEG),                                               \
                 "r"(pc)                                                       \
                 : "eax", "memory");


static inline void must_inline noret
switch_context() {
    asm volatile("jmp do_switch\n");
    unreachable;
}

#define push_arg1(stack_ptr, arg) *((typeof((arg))*)(stack_ptr)--) = arg
#define push_arg2(stack_ptr, arg1, arg2)                                       \
    {                                                                          \
        *((typeof((arg1))*)(stack_ptr)--) = arg1;                              \
        *((typeof((arg2))*)(stack_ptr)--) = arg2;                              \
    }
#define push_arg3(stack_ptr, arg1, arg2, arg3)                                 \
    {                                                                          \
        *((typeof((arg1))*)(stack_ptr)--) = arg1;                              \
        *((typeof((arg2))*)(stack_ptr)--) = arg2;                              \
        *((typeof((arg3))*)(stack_ptr)--) = arg3;                              \
    }
#define push_arg4(stack_ptr, arg1, arg2, arg3, arg4)                           \
    {                                                                          \
        *((typeof((arg1))*)(stack_ptr)--) = arg1;                              \
        *((typeof((arg2))*)(stack_ptr)--) = arg2;                              \
        *((typeof((arg3))*)(stack_ptr)--) = arg3;                              \
        *((typeof((arg4))*)(stack_ptr)--) = arg4;                              \
    }


static inline ptr_t must_inline
abi_get_callframe()
{
    ptr_t val;
    asm("movl %%ebp, %0" : "=r"(val)::);
    return val;
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
