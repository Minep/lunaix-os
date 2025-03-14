#include <asm/aa64_msrs.h>

.section .boot.data
    .align 4
    stack_end:
        .skip 512
    stack_top:

.section .boot.text
    .global start_

    /*
        We follow Linux-arm64 boot protocol
            ldr x0, dtb
            mov x1, xzr
            mov x2, xzr
            mov x3, xzr
     */
    start_:
        adr x4, stack_top
        mov sp, x4
        mov fp, xzr

        mov x19, x0
        bl aarch64_pre_el1_init

        bl aarch64_prepare_el1_transfer
        cbz x0, 1f

        /* transfer to EL1 if we are currently not in */
        adr x0, stack_top
        msr SP_EL1, x0      /* EL1 stack */

        adr x0, 1f
        msr elr, x0         /* PC to resume at EL1, elr aliased to ELR_ELx */

        eret
    1:
        mov x0, x19
        adr x4, aarch64_init
        blr x4
         
        // x0: ptr to boot_handoff
        adr x4, kernel_bootstrap
        blr x4