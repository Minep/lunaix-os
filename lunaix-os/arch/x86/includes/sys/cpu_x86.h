#ifndef __LUNAIX_CPU_X86_H
#define __LUNAIX_CPU_X86_H

#include <lunaix/types.h>

#ifdef CONFIG_ARCH_X86_64
#   define _POP "popq "
#   define _MOV "movq "
#else
#   define _POP "popl "
#   define _MOV "movl "
#endif

/**
 * @brief Load current processor state
 *
 * @return reg_t
 */
static inline reg_t
cpu_ldstate()
{
    ptr_t val;
    asm volatile("pushf\n"
                 _POP "%0\n"
                 : "=r"(val)::);
    return val;
}

/**
 * @brief Load current processor config
 *
 * @return reg_t
 */
static inline reg_t
cpu_ldconfig()
{
    reg_t val;
    asm volatile(_MOV "%%cr0,%0" : "=r"(val));
    return val;
}

/**
 * @brief Change current processor state
 *
 * @return reg_t
 */
static inline void
cpu_chconfig(reg_t val)
{
    asm(_MOV "%0, %%cr0" ::"r"(val));
}

/**
 * @brief Change current virtual memory space
 *
 * @return reg_t
 */
static inline void
cpu_chvmspace(reg_t val)
{
    asm(_MOV "%0, %%cr3" ::"r"(val));
}

/**
 * @brief Read exeception address
 *
 * @return ptr_t
 */
static inline ptr_t
cpu_ldeaddr()
{
    ptr_t val;
    asm volatile(_MOV "%%cr2,%0" : "=r"(val));
    return val;
}

#endif /* __LUNAIX_CPU_X86_H */
