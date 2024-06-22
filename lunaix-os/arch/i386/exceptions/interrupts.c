#include <sys/cpu.h>
#include <sys/i386_intr.h>
#include <sys/hart.h>
#include "sys/x86_isa.h"

#include "sys/x86_isa.h"

#include <lunaix/generic/isrm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/syslog.h>

LOG_MODULE("INTR")

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

void
intr_handler(struct hart_state* state)
{
    update_thread_context(state);

    volatile struct exec_param* execp = state->execp;
    if (execp->vector <= 255) {
        isr_cb subscriber = isrm_get(execp->vector);
        subscriber(state);
        goto done;
    }

    ERROR("INT %u: (%x) [%p: %p] Unknown",
            execp->vector,
            execp->err_code,
            execp->cs,
            execp->eip);

done:

    if (execp->vector > IV_BASE_END) {
        isrm_notify_eoi(0, execp->vector);
    }

    return;
}