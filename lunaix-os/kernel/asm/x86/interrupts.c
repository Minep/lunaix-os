#include <arch/x86/interrupts.h>
#include <lunaix/tty/tty.h>
#include <libc/stdio.h>

void panic_msg(const char* msg) {
    tty_set_theme(VGA_COLOR_WHITE, VGA_COLOR_RED);
    tty_clear_line(10);
    tty_clear_line(11);
    tty_clear_line(12);
    tty_set_cpos(0, 11);
    printf("  %s", msg);
}

void panic (const char* msg, isr_param* param) {
    char buf[1024];
    sprintf(buf, "INT %u: (%x) [%p: %p] %s", param->vector, param->err_code, param->cs, param->eip, msg);
    panic_msg(buf);
    while(1);
}

void 
interrupt_handler(isr_param* param) {
    switch (param->vector)
    {
        case 0:
            panic("Division by 0", param);
            break;  // never reach
        case FAULT_GENERAL_PROTECTION:
            panic("General Protection", param);
            break;  // never reach
        case FAULT_PAGE_FAULT:
            panic("Page Fault", param);
            break;  // never reach
        case LUNAIX_SYS_PANIC:
            panic_msg((char*)(param->registers.edi));
            while(1);
            break;  // never reach
        default:
            panic("Unknown Interrupt", param);
            break;  // never reach
    }
}