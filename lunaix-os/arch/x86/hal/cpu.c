#include <lunaix/types.h>

#include "asm/x86_ivs.h"
#include "asm/x86_cpu.h"

void
cpu_rdmsr(u32_t msr_idx, u32_t* reg_high, u32_t* reg_low)
{
    u32_t h = 0, l = 0;
    asm volatile("rdmsr" : "=d"(h), "=a"(l) : "c"(msr_idx));

    *reg_high = h;
    *reg_low = l;
}

void
cpu_wrmsr(u32_t msr_idx, u32_t reg_high, u32_t reg_low)
{
    asm volatile("wrmsr" : : "d"(reg_high), "a"(reg_low), "c"(msr_idx));
}

void
cpu_trap_sched()
{
    asm("int %0" ::"i"(LUNAIX_SCHED));
}
