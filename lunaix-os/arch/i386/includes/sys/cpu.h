#ifndef __LUNAIX_CPU_H
#define __LUNAIX_CPU_H

#include <lunaix/types.h>

/**
 * @brief Get processor ID string
 *
 * @param id_out
 */
void
cpu_get_id(char* id_out);

void
cpu_trap_sched();

void
cpu_trap_panic(char* message);


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
                 "popl %0\n"
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
    asm volatile("movl %%cr0,%0" : "=r"(val));
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
    asm("mov %0, %%cr0" ::"r"(val));
}

/**
 * @brief Change current virtual memory space
 *
 * @return reg_t
 */
static inline void
cpu_chvmspace(reg_t val)
{
    asm("mov %0, %%cr3" ::"r"(val));
}

static inline void
cpu_enable_interrupt()
{
    asm volatile("sti");
}

static inline void
cpu_disable_interrupt()
{
    asm volatile("cli");
}

static inline void
cpu_wait()
{
    asm("hlt");
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
    asm volatile("movl %%cr2,%0" : "=r"(val));
    return val;
}

#endif /* __LUNAIX_CPU_H */
