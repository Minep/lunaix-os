#include <asm/hart.h>
#include "asm/x86.h"

#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/syslog.h>
#include <lunaix/failsafe.h>

#include <hal/irq.h>

LOG_MODULE("INTR")

extern void
intr_routine_page_fault(const struct hart_state* state);

extern void
syscall_hndlr(const struct hart_state* hstate);

extern void
apic_ack_interrupt(irq_t irq);

static void noret
__set_panic(const char* msg, const struct hart_state* state)
{
    ERROR("panic: %s", msg);
    failsafe_diagnostic();
}

static inline void
update_thread_context(struct hart_state* state)
{
    if (!current_thread) {
        return;
    }

    struct hart_state* parent = current_thread->hstate;
    hart_push_state(parent, state);
    current_thread->hstate = state;

    if (parent) {
        state->depth = parent->depth + 1;
    }
}


static bool
__handle_internal_vectors(const struct hart_state* state)
{
    switch (hart_vector_stamp(state))
    {
    case FAULT_DIVISION_ERROR:
        __set_panic("div zero", state);
        break;

    case FAULT_GENERAL_PROTECTION:
        __set_panic("general protection", state);
        break;

    case FAULT_PAGE_FAULT:
        intr_routine_page_fault(state);
        break;

    case FAULT_INVALID_OPCODE:
        __set_panic("invalid opcode", state);;
        break;

    case LUNAIX_SCHED:
        apic_ack_interrupt(NULL);
        schedule();
        break;

    case APIC_SPIV_IV:
        break;

    case APIC_ERROR_IV:
        __set_panic("apic error", state);;
        break;

    case LUNAIX_SYS_CALL:
        syscall_hndlr(state);
        break;
    
    default:
        return false;
    }

    return true;
}

static bool
__handle_external_irq(const struct hart_state* state)
{
    irq_t irq;
    int ex_iv;

    ex_iv = hart_vector_stamp(state);
    irq   = irq_find(irq_get_default_domain(), ex_iv);

    if (unlikely(!irq)) {
        return false;
    }

    irq_serve(irq, state);
    apic_ack_interrupt(irq);
    return true;
}

struct hart_state*
intr_handler(struct hart_state* state)
{
    update_thread_context(state);

    volatile struct exec_param* execp = state->execp;
    
    if (__handle_internal_vectors(state)) {
        return state;
    }

    if (__handle_external_irq(state)) {
        return state;
    }

    __set_panic("unknown interrupt", state);
}