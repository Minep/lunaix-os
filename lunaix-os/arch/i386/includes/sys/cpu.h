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

static inline ptr_t
cpu_get_fp()
{
    ptr_t val;
    asm("movl %%ebp, %0" : "=r"(val)::);
    return val;
}

/**
 * @brief Load current processor state
 *
 * @return u32_t
 */
static inline u32_t
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
 * @return u32_t
 */
static inline u32_t
cpu_ldconfig()
{
    ptr_t val;
    asm volatile("movl %%cr0,%0" : "=r"(val));
    return val;
}

/**
 * @brief Change current processor state
 *
 * @return u32_t
 */
static inline void
cpu_chconfig(u32_t val)
{
    asm("mov %0, %%cr0" ::"r"(val));
}

/**
 * @brief Load current virtual memory space
 *
 * @return u32_t
 */
static inline u32_t
cpu_ldvmspace()
{
    ptr_t val;
    asm volatile("movl %%cr3,%0" : "=r"(val));
    return val;
}

/**
 * @brief Change current virtual memory space
 *
 * @return u32_t
 */
static inline void
cpu_chvmspace(u32_t val)
{
    asm("mov %0, %%cr3" ::"r"(val));
}

/**
 * @brief Flush TLB
 *
 * @return u32_t
 */
static inline void
cpu_flush_page(ptr_t va)
{
    asm volatile("invlpg (%0)" ::"r"(va) : "memory");
}

static inline void
cpu_flush_vmspace()
{
    asm("movl %%cr3, %%eax\n"
        "movl %%eax, %%cr3" ::
          : "eax");
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

static inline ptr_t
cpu_ldeaddr()
{
    ptr_t val;
    asm volatile("movl %%cr2,%0" : "=r"(val));
    return val;
}

#endif /* __LUNAIX_CPU_H */
