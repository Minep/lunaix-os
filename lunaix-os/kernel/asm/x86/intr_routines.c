#include <arch/x86/interrupts.h>
#include <lunaix/lxconsole.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/tty/tty.h>

#include <klibc/stdio.h>

#include <hal/apic.h>

LOG_MODULE("INTR")

extern void
intr_routine_page_fault(const isr_param* param);

extern uint32_t debug_resv;

void
__print_panic_msg(const char* msg, const isr_param* param)
{
    kprint_panic("  INT %u: (%x) [%p: %p] %s",
                 param->vector,
                 param->err_code,
                 param->cs,
                 param->eip,
                 msg);
}

void
intr_routine_divide_zero(const isr_param* param)
{
    __print_panic_msg("Divide by zero!", param);
    spin();
}

void
intr_routine_general_protection(const isr_param* param)
{
    kprintf(KERROR "Pid: %d\n", __current->pid);
    kprintf(KERROR "Addr: %p\n", (&debug_resv)[0]);
    kprintf(KERROR "Expected: %p\n", (&debug_resv)[1]);
    console_flush(0);
    __print_panic_msg("General Protection", param);
    spin();
}

void
intr_routine_sys_panic(const isr_param* param)
{
    __print_panic_msg((char*)(param->registers.edi), param);
    spin();
}

void
intr_routine_fallback(const isr_param* param)
{
    __print_panic_msg("Unknown Interrupt", param);
    spin();
}

/**
 * @brief ISR for Spurious interrupt
 *
 * @param isr_param passed by CPU
 */
void
intr_routine_apic_spi(const isr_param* param)
{
    // FUTURE: do nothing for now
}

void
intr_routine_apic_error(const isr_param* param)
{
    uint32_t error_reg = apic_read_reg(APIC_ESR);
    char buf[32];
    sprintf(buf, "APIC error, ESR=0x%x", error_reg);
    __print_panic_msg(buf, param);
    spin();
}

void
intr_routine_init()
{
    intr_subscribe(FAULT_DIVISION_ERROR, intr_routine_divide_zero);
    intr_subscribe(FAULT_GENERAL_PROTECTION, intr_routine_general_protection);
    intr_subscribe(FAULT_PAGE_FAULT, intr_routine_page_fault);
    intr_subscribe(FAULT_STACK_SEG_FAULT, intr_routine_page_fault);
    intr_subscribe(LUNAIX_SYS_PANIC, intr_routine_sys_panic);
    intr_subscribe(APIC_SPIV_IV, intr_routine_apic_spi);
    intr_subscribe(APIC_ERROR_IV, intr_routine_apic_error);

    intr_set_fallback_handler(intr_routine_fallback);
}