#define __ASM__
#include <arch/x86/interrupts.h>
// #define __ASM_INTR_DIAGNOSIS

.macro isr_template vector, no_error_code=1
    .global _asm_isr\vector
    .type _asm_isr\vector, @function
    _asm_isr\vector:
        .if \no_error_code
            pushl $0x0
        .endif
        pushl $\vector
        jmp interrupt_wrapper
.endm

#ifdef __ASM_INTR_DIAGNOSIS
.section .bss
    .global debug_resv
    debug_resv:
        .skip 16
#endif

.section .text
    isr_template FAULT_DIVISION_ERROR
    isr_template FAULT_GENERAL_PROTECTION, no_error_code=0
    isr_template FAULT_PAGE_FAULT, no_error_code=0

    isr_template LUNAIX_SYS_PANIC
    isr_template LUNAIX_SYS_CALL

    isr_template APIC_ERROR_IV
    isr_template APIC_LINT0_IV
    isr_template APIC_TIMER_IV
    isr_template APIC_SPIV_IV
    isr_template RTC_TIMER_IV
    isr_template PC_KBD_IV

    interrupt_wrapper:
        pushl %esp

        pushl %esi
        pushl %ebp
        pushl %edi
        pushl %edx
        pushl %ecx
        pushl %ebx
        pushl %eax

        movl %esp, %eax
        andl $0xfffffff0, %esp
        subl $16, %esp
        movl %eax, (%esp)

        call intr_handler

    .global soft_iret
    soft_iret:
        popl %esp

        popl %eax
        popl %ebx
        popl %ecx
        popl %edx
        popl %edi
        popl %ebp
        popl %esi
        popl %esp

        addl $8, %esp

#ifdef __ASM_INTR_DIAGNOSIS
        cmpl $0, (%esp)
        jz 1f
        iret
1:
        movl $__current, %eax
        movl  (%esp), %ebx
        movl $debug_resv, %ecx
        ud2
#else
        iret
#endif
