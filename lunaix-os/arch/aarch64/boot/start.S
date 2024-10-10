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

        adr x4, aarch64_init
        bl x4
         
        // x0: ptr to boot_handoff
        adr x4, kernel_bootstrap
        bl x4