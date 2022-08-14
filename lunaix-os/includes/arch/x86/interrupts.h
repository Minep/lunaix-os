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

#pragma region ISR_DECLARATION

#define ISR(iv) void _asm_isr##iv();

ISR(0)
ISR(1)
ISR(2)
ISR(3)
ISR(4)
ISR(5)
ISR(6)
ISR(7)
ISR(8)
ISR(9)
ISR(10)
ISR(11)
ISR(12)
ISR(13)
ISR(14)
ISR(15)
ISR(16)
ISR(17)
ISR(18)
ISR(19)
ISR(20)
ISR(21)

ISR(32)
ISR(33)
ISR(34)

ISR(201)
ISR(202)

ISR(210)

ISR(250)
ISR(251)
ISR(252)
ISR(253)
ISR(254)

#pragma endregion

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
