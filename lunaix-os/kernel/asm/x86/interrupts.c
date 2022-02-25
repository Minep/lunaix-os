#include <arch/x86/interrupts.h>
#include <libc/stdio.h>

void isr0 (isr_param* param) {
    tty_clear();
    printf("[PANIC] Exception (%d) CS=0x%X, EIP=0x%X", param->vector, param->cs, param->eip);
}

void 
interrupt_handler(isr_param* param) {
    switch (param->vector)
    {
        case 0:
            isr0(param);
            break;
    }
}