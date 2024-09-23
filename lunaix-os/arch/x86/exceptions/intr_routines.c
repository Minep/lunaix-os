#include <asm/hart.h>

#include <asm/isrm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/trace.h>
#include <lunaix/failsafe.h>

#include <klibc/strfmt.h>

#include "asm/soc/apic.h"
#include "asm/x86.h"

LOG_MODULE("INTR")

extern void
intr_routine_page_fault(const struct hart_state* state);

extern u32_t debug_resv;

void
__print_panic_msg(const char* msg, const struct hart_state* state)
{
    ERROR("panic: %s", msg);
    failsafe_diagnostic();
}

void
intr_routine_divide_zero(const struct hart_state* state)
{
    __print_panic_msg("div zero", state);
}

void
intr_routine_general_protection(const struct hart_state* state)
{
    __print_panic_msg("general protection", state);
}

void
intr_routine_invl_opcode(const struct hart_state* state)
{
    __print_panic_msg("invalid opcode", state);
}

void
intr_routine_sys_panic(const struct hart_state* state)
{
#ifdef CONFIG_ARCH_X86_64
    __print_panic_msg((char*)(state->registers.rdi), state);
#else
    __print_panic_msg((char*)(state->registers.edi), state);
#endif
}

void
intr_routine_fallback(const struct hart_state* state)
{
    __print_panic_msg("unknown interrupt", state);
}

/**
 * @brief ISR for Spurious interrupt
 *
 * @param struct hart_state passed by CPU
 */
void
intr_routine_apic_spi(const struct hart_state* state)
{
    // FUTURE: do nothing for now
}

void
intr_routine_apic_error(const struct hart_state* state)
{
    u32_t error_reg = apic_read_reg(APIC_ESR);
    char buf[32];
    ksprintf(buf, "APIC error, ESR=0x%x", error_reg);

    failsafe_diagnostic();
}

void
intr_routine_sched(const struct hart_state* state)
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
    isrm_bindiv(FAULT_INVALID_OPCODE, intr_routine_invl_opcode);

    isrm_bindiv(LUNAIX_SYS_PANIC, intr_routine_sys_panic);
    isrm_bindiv(LUNAIX_SCHED, intr_routine_sched);

    isrm_bindiv(APIC_SPIV_IV, intr_routine_apic_spi);
    isrm_bindiv(APIC_ERROR_IV, intr_routine_apic_error);
}