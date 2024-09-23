#ifndef __LUNAIX_ARCH_CPU_H
#define __LUNAIX_ARCH_CPU_H

#include <lunaix/types.h>

void
cpu_trap_sched();

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

#endif /* __LUNAIX_CPU_H */
