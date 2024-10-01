#define __ASM__
#include <asm/aa64_exception.h>
#include <asm/aa64_msrs.h>

.macro aa64_exception_entry type
    .align 128
    .type  _exception_entry_t\type, @function

    # each handler has at most 128 bytes (32 insts)
    _exception_entry_t\type:
        stp x1,  x0,  [sp, #-16]

        # re-purpose the [63, 56] of ESR to exception
        #  type identifier

        mov x0,  =\type
        lsl x0,  x0, #56
        mrs x1,  ESR_EL1
        orr x0,  x0, x1

        ldr x1,  [sp, #-8]
        str x0,  [sp, #-8]!
        ldr x0,  [sp, #-8]

        b _aa64_evec_prehandle
.endm

.section .text

    .globl aa64_vbase
    .align 128

    aa64_vbase:
    aa64_exception_entry EXCEPTION_SYNC
    aa64_exception_entry EXCEPTION_IRQ
    aa64_exception_entry EXCEPTION_FIQ
    aa64_exception_entry EXCEPTION_SERR
