#ifndef __LUNAIX_OWLOYSIUS_H
#define __LUNAIX_OWLOYSIUS_H

#include <lunaix/ds/ldga.h>

/**
 * @brief stage where only basic memory management service
 * is present
 */
#define on_sysconf c_sysconf

/**
 * @brief stage where basic memory management service
 * interrupt management and timer/clock service avaliable
 */
#define on_earlyboot c_earlyboot

/**
 * @brief stage where most kernel service is ready, non-preempt 
 * kernel.
 * 
 * boot-stage initialization is about to conclude.
 */
#define on_boot c_boot

/**
 * @brief stage where all services started, kernel is in preempt
 * state
 */
#define on_postboot c_postboot

#define owloysius_fetch_init(func, call_stage)                                     \
    export_ldga_el(lunainit, func, ptr_t, func);                            \
    export_ldga_el_sfx(lunainit, func##_##call_stage, ptr_t, func, call_stage);

#define invoke_init_function(stage) ldga_invoke_fn0(lunainit##_##stage)

static inline void
initfn_invoke_sysconf()
{
    invoke_init_function(on_sysconf);
}

static inline void
initfn_invoke_earlyboot()
{
    invoke_init_function(on_earlyboot);
}

static inline void
initfn_invoke_boot()
{
    invoke_init_function(on_boot);
}

static inline void
initfn_invoke_postboot()
{
    invoke_init_function(on_postboot);
}

#endif /* __LUNAIX_OWLOYSIUS_H */
