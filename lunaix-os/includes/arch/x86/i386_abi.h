#ifndef __LUNAIX_I386ABI_H
#define __LUNAIX_I386ABI_H

#define store_retval(retval) __current->intr_ctx->registers.eax = (retval)

#define store_retval_to(proc, retval) (proc)->intr_ctx->registers.eax = (retval)

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

#endif /* __LUNAIX_ABI_H */
