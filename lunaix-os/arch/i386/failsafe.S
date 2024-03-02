
.section .bss.kstack
    .skip 1024
    .align 16
    .global failsafe_stack_top
    failsafe_stack_top:
