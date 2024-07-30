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
 * @return ptr_t
 */
ptr_t
cpu_ldstate();

/**
 * @brief Load current processor config
 *
 * @return ptr_t
 */
ptr_t
cpu_ldconfig();

/**
 * @brief Change current processor state
 *
 * @return ptr_t
 */
void
cpu_chconfig(ptr_t val);

/**
 * @brief Change current virtual memory space
 *
 * @return ptr_t
 */
void
cpu_chvmspace(ptr_t val);

void
cpu_enable_interrupt();

void
cpu_disable_interrupt();

void
cpu_wait();

/**
 * @brief Read exeception address
 *
 * @return ptr_t
 */
ptr_t
cpu_ldeaddr();


#endif /* __LUNAIX_CPU_H */
