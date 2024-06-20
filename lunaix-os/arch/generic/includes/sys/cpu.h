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
static inline ptr_t
cpu_ldstate()
{
    return 0;
}

/**
 * @brief Load current processor config
 *
 * @return ptr_t
 */
static inline ptr_t
cpu_ldconfig()
{
    return 0;
}

/**
 * @brief Change current processor state
 *
 * @return ptr_t
 */
static inline void
cpu_chconfig(ptr_t val)
{
    return;
}

/**
 * @brief Change current virtual memory space
 *
 * @return ptr_t
 */
static inline void
cpu_chvmspace(ptr_t val)
{
    return;
}

static inline void
cpu_enable_interrupt()
{
    return;
}

static inline void
cpu_disable_interrupt()
{
    return;
}

static inline void
cpu_wait()
{
    return;
}

/**
 * @brief Read exeception address
 *
 * @return ptr_t
 */
static inline ptr_t
cpu_ldeaddr()
{
    return 0;
}


#endif /* __LUNAIX_CPU_H */
