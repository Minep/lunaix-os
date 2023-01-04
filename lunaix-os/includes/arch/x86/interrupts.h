#ifndef __LUNAIX_INTERRUPTS_H
#define __LUNAIX_INTERRUPTS_H

#include "vectors.h"

#ifndef __ASM__
#include <hal/cpu.h>

struct exec_param;

typedef struct
{
    unsigned int depth;
    struct
    {
        reg32 eax;
        reg32 ebx;
        reg32 ecx;
        reg32 edx;
        reg32 edi;
        reg32 ebp;
        reg32 esi;
        reg32 ds;
        reg32 es;
        reg32 fs;
        reg32 gs;
    } __attribute__((packed)) registers;

    union
    {
        reg32 esp;
        volatile struct exec_param* execp;
    };
} __attribute__((packed)) isr_param;

struct exec_param
{
    isr_param saved_prev_ctx;
    unsigned int vector;
    unsigned int err_code;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int esp;
    unsigned int ss;
} __attribute__((packed));

#define ISR_PARAM_SIZE sizeof(isr_param)

void
intr_handler(isr_param* param);

void
intr_routine_init();

#endif

#endif /* __LUNAIX_INTERRUPTS_H */
