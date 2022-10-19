#ifndef __LUNAIX_VECTORS_H
#define __LUNAIX_VECTORS_H

// clang-format off

#define FAULT_DIVISION_ERROR            0
#define INSTR_DEBUG                     1
#define INT_NMI                         2
#define INSTR_BREAK                     3
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
#define LUNAIX_SYS_CALL                 33
#define LUNAIX_SCHED                    34

#define EX_INTERRUPT_BEGIN              200

// Keyboard
#define PC_KBD_IV                       201
#define AHCI_HBA_IV                     202
#define UART_COM1                       203

#define RTC_TIMER_IV                    210

// 来自APIC的中断有着最高的优先级。
// APIC related
#define APIC_ERROR_IV                   250
#define APIC_LINT0_IV                   251
#define APIC_SPIV_IV                    252
#define APIC_TIMER_IV                   253

#define PC_AT_IRQ_RTC                   8
#define PC_AT_IRQ_KBD                   1

// clang-format on

#endif /* __LUNAIX_VECTORS_H */
