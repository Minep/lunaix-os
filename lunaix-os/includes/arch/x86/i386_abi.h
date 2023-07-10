#ifndef __LUNAIX_I386ABI_H
#define __LUNAIX_I386ABI_H

#define store_retval(retval) __current->intr_ctx.registers.eax = (retval)

#define store_retval_to(proc, retval) (proc)->intr_ctx.registers.eax = (retval)

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

#endif /* __LUNAIX_ABI_H */
