#ifndef __LUNAIX_ARCH_CPU_H
#define __LUNAIX_ARCH_CPU_H

#include "aa64.h"

void
cpu_trap_sched();

static inline void
cpu_enable_interrupt()
{
    set_sysreg(ALLINT_EL1, 0);
}

static inline void
cpu_disable_interrupt()
{
    set_sysreg(ALLINT_EL1, 1 << 12);
}

static inline void
cpu_wait()
{
    asm volatile ( "wfi" );
}

#endif /* __LUNAIX_CPU_H */
