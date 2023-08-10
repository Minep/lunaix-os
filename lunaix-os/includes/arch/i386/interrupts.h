#ifndef __LUNAIX_INTERRUPTS_H
#define __LUNAIX_INTERRUPTS_H

#include "vectors.h"

#ifndef __ASM__
#include <hal/cpu.h>

struct exec_param;

struct regcontext
{
    u32_t eax;
    u32_t ebx;
    u32_t ecx;
    u32_t edx;
    u32_t edi;
    u32_t ebp;
    u32_t esi;
    u32_t ds;
    u32_t es;
    u32_t fs;
    u32_t gs;
} __attribute__((packed));

typedef struct
{
    unsigned int depth;
    struct regcontext registers;
    union
    {
        u32_t esp;
        volatile struct exec_param* execp;
    };
} __attribute__((packed)) isr_param;

struct exec_param
{
    isr_param* saved_prev_ctx;
    u32_t vector;
    u32_t err_code;
    u32_t eip;
    u32_t cs;
    u32_t eflags;
    u32_t esp;
    u32_t ss;
} __attribute__((packed));

#define ISR_PARAM_SIZE sizeof(isr_param)

void
intr_handler(isr_param* param);

#endif

#endif /* __LUNAIX_INTERRUPTS_H */
