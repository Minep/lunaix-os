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

/**
 * @brief Load current processor state
 *
 * @return u32_t
 */
u32_t
cpu_ldstate();

/**
 * @brief Load current processor config
 *
 * @return u32_t
 */
u32_t
cpu_ldconfig();

/**
 * @brief Change current processor state
 *
 * @return u32_t
 */
void
cpu_chconfig(u32_t val);

/**
 * @brief Load current virtual memory space
 *
 * @return u32_t
 */
u32_t
cpu_ldvmspace();

/**
 * @brief Change current virtual memory space
 *
 * @return u32_t
 */
void
cpu_chvmspace(u32_t val);

/**
 * @brief Flush TLB
 *
 * @return u32_t
 */
void
cpu_flush_page(ptr_t va);

void
cpu_flush_vmspace();

void
cpu_enable_interrupt();

void
cpu_disable_interrupt();

void
cpu_trap_sched();

void
cpu_trap_panic(char* message);

void
cpu_wait();

ptr_t
cpu_ldeaddr();

#endif /* __LUNAIX_CPU_H */
