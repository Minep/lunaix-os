#include <asm/hart.h>
#include "asm/x86.h"

#include <asm-generic/isrm.h>

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

struct hart_state*
intr_handler(struct hart_state* state)
{
    update_thread_context(state);

    volatile struct exec_param* execp = state->execp;
    if (execp->vector <= 255) {
        isr_cb subscriber = isrm_get(execp->vector);
        subscriber(state);
        goto done;
    }

done:

    if (execp->vector > IV_BASE_END) {
        isrm_notify_eoi(0, execp->vector);
    }

    return state;
}