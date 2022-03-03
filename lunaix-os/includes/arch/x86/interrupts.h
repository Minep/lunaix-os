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

#define LUNAIX_SYS_PANIC                32

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

void
_asm_isr0();

void
_asm_isr1();

void
_asm_isr2();

void
_asm_isr3();

void
_asm_isr4();

void
_asm_isr5();

void
_asm_isr6();

void
_asm_isr7();

void
_asm_isr8();

void
_asm_isr9();

void
_asm_isr10();

void
_asm_isr11();

void
_asm_isr12();

void
_asm_isr13();

void
_asm_isr14();

void
_asm_isr15();

void
_asm_isr16();

void
_asm_isr17();

void
_asm_isr18();

void
_asm_isr19();

void
_asm_isr20();

void
_asm_isr21();


void
_asm_isr32();

void
interrupt_handler(isr_param* param);

#endif

#endif /* __LUNAIX_INTERRUPTS_H */
