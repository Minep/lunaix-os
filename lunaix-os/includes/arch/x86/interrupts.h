#ifndef __LUNAIX_INTERRUPTS_H
#define __LUNAIX_INTERRUPTS_H


#define FAULT_DIVISION_ERROR            0
#define FAULT_TRAP_DEBUG_EXCEPTION      1
#define INT_NMI                         2
#define TRAP_BREAKPOINT                 3
#define TRAP_OVERFLOW                   4
#define FAULT_BOUND_EXCEED              5
#define FAULT_INVALID_OPCODE            6
#define FAULT_NO_MATH_PROCESSOR         7
#define ABORT_DOUBLE_FAULT              8
#define FAULT_RESERVED_0                9
#define FAULT_INVALID_TSS               10
#define FAULT_SEG_NOT_PRESENT           11
#define FAULT_STACK_SEG_FAULT           12
#define FAULT_GENERAL_PROTECTION        13
#define FAULT_PAGE_FAULT                14
#define FAULT_RESERVED_1                15
#define FAULT_X87_FAULT                 16
#define FAULT_ALIGNMENT_CHECK           17
#define ABORT_MACHINE_CHECK             18
#define FAULT_SIMD_FP_EXCEPTION         19
#define FAULT_VIRTUALIZATION_EXCEPTION  20
#define FAULT_CONTROL_PROTECTION        21

// LunaixOS related
#define LUNAIX_SYS_PANIC                32

#define EX_INTERRUPT_BEGIN              200

// Keyboard
#define PC_KBD_IV                       201

#define RTC_TIMER_IV                    210

// 来自APIC的中断有着最高的优先级。
// APIC related
#define APIC_ERROR_IV                   250
#define APIC_LINT0_IV                   251
#define APIC_SPIV_IV                    252
#define APIC_TIMER_IV                   253

#define PC_AT_IRQ_RTC                   8
#define PC_AT_IRQ_KBD                   1

#ifndef __ASM__
#include <hal/cpu.h>
typedef struct {
    gp_regs registers;
    unsigned int vector;
    unsigned int err_code;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int esp;
    unsigned int ss;
} __attribute__((packed)) isr_param;

typedef void (*int_subscriber)(isr_param*);

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

ISR(201)

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

void
intr_set_fallback_handler(int_subscriber);

void
intr_handler(isr_param* param);

void
intr_routine_init();

#endif

#endif /* __LUNAIX_INTERRUPTS_H */
