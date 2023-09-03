#include <sys/interrupts.h>

#include <lunaix/isrm.h>
#include <lunaix/lxconsole.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/trace.h>

#include <klibc/stdio.h>

#include <sys/apic.h>
#include <sys/i386_intr.h>

LOG_MODULE("INTR")

extern void
intr_routine_page_fault(const isr_param* param);

extern u32_t debug_resv;

void
__print_panic_msg(const char* msg, const isr_param* param)
{
    kprintf(KERROR "panic: %s\n", msg);
    trace_printstack_isr(param);
}

void
intr_routine_divide_zero(const isr_param* param)
{
    __print_panic_msg("div zero", param);

    console_flush();
    spin();
}

void
intr_routine_general_protection(const isr_param* param)
{
    __print_panic_msg("general protection", param);

    console_flush();
    spin();
}

void
intr_routine_sys_panic(const isr_param* param)
{
    __print_panic_msg((char*)(param->registers.edi), param);

    console_flush();
    spin();
}

void
intr_routine_fallback(const isr_param* param)
{
    __print_panic_msg("unknown interrupt", param);

    console_flush();
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
    u32_t error_reg = apic_read_reg(APIC_ESR);
    char buf[32];
    ksprintf(buf, "APIC error, ESR=0x%x", error_reg);

    __print_panic_msg(buf, param);

    console_flush();
    spin();
}

void
intr_routine_sched(const isr_param* param)
{
    schedule();
}

void
intr_routine_init()
{
    isrm_bindiv(FAULT_DIVISION_ERROR, intr_routine_divide_zero);
    isrm_bindiv(FAULT_GENERAL_PROTECTION, intr_routine_general_protection);
    isrm_bindiv(FAULT_PAGE_FAULT, intr_routine_page_fault);
    isrm_bindiv(FAULT_STACK_SEG_FAULT, intr_routine_page_fault);

    isrm_bindiv(LUNAIX_SYS_PANIC, intr_routine_sys_panic);
    isrm_bindiv(LUNAIX_SCHED, intr_routine_sched);

    isrm_bindiv(APIC_SPIV_IV, intr_routine_apic_spi);
    isrm_bindiv(APIC_ERROR_IV, intr_routine_apic_error);
}