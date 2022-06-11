#define __ASM__
#include <arch/x86/interrupts.h>
#include <lunaix/common.h>
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
        /*
         Stack layout
    msa:   [ss]
           [esp]
           eflags
           cs
           eip
           err_code   
           vector     > offset = 28 + 16 + 4 = 48
           esp
           gs
           fs
           es
           ds         > offset = 7 * 4 = 28
           esi
           ebp
           edi
           edx
           ecx
           ebx
    lsa:   eax        > offset = 0

            las: Least Significant Address
            msa: Most Significant Address
        */
        pushl %esp

        subl $16, %esp
        movw %gs, 12(%esp)
        movw %fs,  8(%esp)
        movw %es,  4(%esp)
        movw %ds,   (%esp)

        pushl %esi
        pushl %ebp
        pushl %edi
        pushl %edx
        pushl %ecx
        pushl %ebx
        pushl %eax

        movl 60(%esp), %eax   /* 取出 %cs */
        andl $0x3, %eax          /* 判断 RPL */
        jz 1f

        movw $KDATA_SEG, %ax    /* 如果从用户模式转来，则切换至内核数据段 */
        movw %ax, %gs
        movw %ax, %fs
        movw %ax, %ds
        movw %ax, %es

    1:
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
        
        movw   (%esp), %ds
        movw  4(%esp), %es
        movw  8(%esp), %fs
        movw 12(%esp), %gs

        movl 16(%esp), %esp

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
