#include <arch/x86/interrupts.h>

#include <lunaix/isrm.h>
#include <lunaix/lxconsole.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/tty/tty.h>

#include <klibc/stdio.h>

#include <hal/apic.h>

LOG_MODULE("INTR")

extern void
intr_routine_page_fault(const isr_param* param);

extern u32_t debug_resv;

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
    console_flush();
    __print_panic_msg("Divide by zero!", param);
    spin();
}

void
intr_routine_general_protection(const isr_param* param)
{
    kprintf(KERROR "Pid: %d\n", __current->pid);
    kprintf(KERROR "Addr: %p\n", (&debug_resv)[0]);
    kprintf(KERROR "Expected: %p\n", (&debug_resv)[1]);
    console_flush();
    __print_panic_msg("General Protection", param);
    spin();
}

void
intr_routine_sys_panic(const isr_param* param)
{
    console_flush();
    __print_panic_msg((char*)(param->registers.edi), param);
    spin();
}

void
intr_routine_fallback(const isr_param* param)
{
    console_flush();
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
    u32_t error_reg = apic_read_reg(APIC_ESR);
    char buf[32];
    ksprintf(buf, "APIC error, ESR=0x%x", error_reg);
    console_flush();
    __print_panic_msg(buf, param);
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