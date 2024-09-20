#ifndef __LUNAIX_CPU_H
#define __LUNAIX_CPU_H

#include <lunaix/types.h>

void
cpu_trap_sched();

void
cpu_enable_interrupt();

void
cpu_disable_interrupt();

void
cpu_wait();

#endif /* __LUNAIX_CPU_H */
