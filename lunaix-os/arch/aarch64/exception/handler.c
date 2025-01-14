#include <lunaix/process.h>
#include <asm/hart.h>
#include <asm/aa64_exception.h>

extern void 
handle_mm_abort(struct hart_state* state);

extern void
aa64_syscall(struct hart_state* hstate);

static inline void
update_thread_context(struct hart_state* state)
{
    if (!current_thread) {
        return;
    }

    struct hart_state* parent = current_thread->hstate;
    hart_push_state(parent, state);

    current_thread->hstate = state;
    current_thread->ustack_top = state->execp.sp_el0;

    if (parent) {
        state->depth = parent->depth + 1;
    }
}

static inline void
handle_sync_exception(struct hart_state* hstate)
{
    unsigned int ec;

    ec = esr_ec(hstate->execp.syndrome);

    switch (ec)
    {
    case EC_I_ABORT:
    case EC_D_ABORT:
    case EC_I_ABORT_EL:
    case EC_D_ABORT_EL:
        handle_mm_abort(hstate);
        break;
    
    case EC_SVC:
        aa64_syscall(hstate);
        break;
    
    default:
        fail("unhandled exception (synced)");
        break;
    }
}

static inline void
handle_async_exception(struct hart_state* hstate)
{
    int err = 0;
    
    err = gic_handle_irq(hstate);
    if (!err) {
        return;
    }

    // TODO do we have other cases of async exception?
}

struct hart_state*
handle_exception(struct hart_state* hstate)
{
    update_thread_context(hstate);

    if (hart_vector_stamp(hstate) == EXCEPTION_SYNC) {
        handle_sync_exception(hstate);
    } else {
        handle_async_exception(hstate);
    }

    return hstate;
}