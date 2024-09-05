.section .boot.data
    .align 16
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
        ldr sp, =stack_top
        mov fp, xzr

        ldr x4, =aarch64_init
        blx x4
         
        // x0: ptr to boot_handoff
        ldr x4, =kernel_bootstrap
        blx x4