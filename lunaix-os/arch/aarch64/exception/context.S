#define __ASM__
#include <asm/aa64_msrs.h>
#include "hart_field.inc"

.section .text
    
    .globl _aa64_evec_prehandle
    .globl _aa64_switch_task

    _aa64_evec_prehandle:
        # reservation for struct exec_param
        sub   sp,  sp,  #8         # push  sp_el1
        sub   sp,  sp,  #16        # push {sp_el0, link}
        sub   sp,  sp,  #16        # push {spsr,   parent_hart}

        stp   lr,  fp, [sp, #-16]!  # push {x31-x1}
        stp  x28, x27, [sp, #-16]!
        stp  x26, x25, [sp, #-16]!
        stp  x24, x23, [sp, #-16]!
        stp  x22, x21, [sp, #-16]!
        stp  x20, x19, [sp, #-16]!
        stp  x18, x17, [sp, #-16]!
        stp  x16, x15, [sp, #-16]!
        stp  x14, x13, [sp, #-16]!
        stp  x12, x11, [sp, #-16]!
        stp  x10,  x9, [sp, #-16]!
        stp   x8,  x7, [sp, #-16]!
        stp   x6,  x5, [sp, #-16]!
        stp   x4,  x3, [sp, #-16]!
        stp   x2,  x1, [sp, #-16]!
        stp   x0, xzr, [sp, #-16]!  # push {x0, xzr}

        add   x1, sp,  #hart_execp

        mrs   x0, SP_EL0
        str   x0, [x1, #execp_spel0_saved]

        mrs   x0, ELR_E1
        str   x0, [x1, #execp_link]

        mrs   x0, SPSR_EL1
        str   x0, [x1, #execp_spsr]
        
        sub   x0, sp,  #hart_end
        str   x0, [x1, #execp_spel1_saved]
        
        mov   x0, sp
        bl    handle_exception
        
    do_eret:
        mov   sp,   x0

        add   x1,   x0, #hart_execp

        ldr   x0,  [x1, #execp_spsr]
        msr   SPSR_EL1, x0

        ldr   x0,  [x1, #execp_link]
        msr   ELR_E1,   x0

        ldr   x0,  [x1, #execp_spel0_saved]
        msr   SP_EL0,   x0

        add   sp,  sp, #hart_regs

        ldr   x0,      [sp, #8 ]!
        ldp   x1,  x2, [sp, #16]!
        ldp   x3,  x4, [sp, #16]!
        ldp   x5,  x6, [sp, #16]!
        ldp   x7,  x8, [sp, #16]!
        ldp   x9, x10, [sp, #16]!
        ldp  x11, x12, [sp, #16]!
        ldp  x13, x14, [sp, #16]!
        ldp  x15, x16, [sp, #16]!
        ldp  x17, x18, [sp, #16]!
        ldp  x19, x20, [sp, #16]!
        ldp  x21, x22, [sp, #16]!
        ldp  x23, x24, [sp, #16]!
        ldp  x25, x26, [sp, #16]!
        ldp  x27, x28, [sp, #16]!
        ldp   fp,  lr, [sp, #16]!
        
        # sp now point to the start of exec_param

        ldr   sp,  [sp, #execp_spel1_saved]

        eret

    _aa64_switch_task:
        # TODO
        b     do_eret