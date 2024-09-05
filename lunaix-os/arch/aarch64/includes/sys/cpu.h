#ifndef __LUNAIX_AA64_CPU_H
#define __LUNAIX_AA64_CPU_H

#include <sys/msrs.h>

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

#endif /* __LUNAIX_AA64_CPU_H */
