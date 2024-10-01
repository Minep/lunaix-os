#include <lunaix/process.h>
#include <asm/hart.h>

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
handle_exception(struct hart_state* hstate)
{
    update_thread_context(hstate);

    return hstate;
}