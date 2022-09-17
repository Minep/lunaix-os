#ifndef __LUNAIX_INTERRUPTS_H
#define __LUNAIX_INTERRUPTS_H

#include "vectors.h"

#ifndef __ASM__
#include <hal/cpu.h>
typedef struct
{
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
        reg32 esp;
    } registers;

    unsigned int vector;
    unsigned int err_code;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int esp;
    unsigned int ss;
} __attribute__((packed)) isr_param;

typedef void (*int_subscriber)(const isr_param*);

void
intr_subscribe(const uint8_t vector, int_subscriber);

void
intr_unsubscribe(const uint8_t vector, int_subscriber);

void intr_set_fallback_handler(int_subscriber);

void
intr_handler(isr_param* param);

void
intr_routine_init();

#endif

#endif /* __LUNAIX_INTERRUPTS_H */
