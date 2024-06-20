#ifndef __LUNAIX_ARCH_VECTORS_H
#define __LUNAIX_ARCH_VECTORS_H

#define TOTAL_IV 0

#define FAULT_DIVISION_ERROR            0
#define INSTR_DEBUG                     0
#define INT_NMI                         0
#define INSTR_BREAK                     0
#define TRAP_OVERFLOW                   0
#define FAULT_BOUND_EXCEED              0
#define FAULT_INVALID_OPCODE            0
#define FAULT_NO_MATH_PROCESSOR         0
#define ABORT_DOUBLE_FAULT              0
#define FAULT_RESERVED_0                0
#define FAULT_INVALID_TSS               0
#define FAULT_SEG_NOT_PRESENT           0
#define FAULT_STACK_SEG_FAULT           0
#define FAULT_GENERAL_PROTECTION        0
#define FAULT_PAGE_FAULT                0
#define FAULT_RESERVED_1                0
#define FAULT_X87_FAULT                 0
#define FAULT_ALIGNMENT_CHECK           0
#define ABORT_MACHINE_CHECK             0
#define FAULT_SIMD_FP_EXCEPTION         0
#define FAULT_VIRTUALIZATION_EXCEPTION  0
#define FAULT_CONTROL_PROTECTION        0

#define IV_BASE_END       0

// LunaixOS related
#define LUNAIX_SYS_PANIC                0
#define LUNAIX_SYS_CALL                 0

// begin allocatable iv resources
#define IV_EX_BEGIN                     0
#define LUNAIX_SCHED                    0

// end allocatable iv resources
#define IV_EX_END             249

#endif /* __LUNAIX_ARCH_VECTORS_H */
