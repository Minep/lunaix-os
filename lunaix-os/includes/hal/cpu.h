#ifndef __LUNAIX_CPU_H
#define __LUNAIX_CPU_H

#include <stdint.h>

typedef unsigned int reg32;
typedef unsigned short reg16;

typedef struct
{
    reg32 eax;
    reg32 ebx;
    reg32 ecx;
    reg32 edx;
    reg32 edi;
    reg32 ebp;
    reg32 esi;
    reg32 esp;
} __attribute__((packed)) gp_regs;

typedef struct
{
    reg16 ss;
    reg16 es;
    reg16 ds;
    reg16 fs;
    reg16 gs;
    reg16 cs;
} __attribute__((packed)) sg_reg;

void
cpu_get_brand(char* brand_out);

int
cpu_has_apic();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
static inline reg32
cpu_rcr0()
{
    asm("mov %cr0, %eax");
}

static inline reg32
cpu_rcr2()
{
    asm("mov %cr2, %eax");
}

static inline reg32
cpu_rcr3()
{
    asm("mov %cr3, %eax");
}
#pragma GCC diagnostic pop

static inline void
cpu_lcr0(reg32 v)
{
    asm("mov %0, %%cr0" ::"r"(v));
}

static inline void
cpu_lcr2(reg32 v)
{
    asm("mov %0, %%cr2" ::"r"(v));
}

static inline void
cpu_lcr3(reg32 v)
{
    asm("mov %0, %%cr3" ::"r"(v));
}

static inline void
cpu_invplg(void* va)
{
    asm volatile("invlpg (%0)" ::"r"((uintptr_t)va) : "memory");
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
cpu_invtlb()
{
    reg32 interm;
    asm("movl %%cr3, %0\n"
        "movl %0, %%cr3"
        : "=r"(interm)
        : "r"(interm));
}

void
cpu_rdmsr(uint32_t msr_idx, uint32_t* reg_high, uint32_t* reg_low);

void
cpu_wrmsr(uint32_t msr_idx, uint32_t reg_high, uint32_t reg_low);

#endif