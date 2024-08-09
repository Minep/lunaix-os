#include <lunaix/switch.h>
#include <lunaix/signal.h>
#include <lunaix/sched.h>
#include <lunaix/process.h>

extern void
signal_dispatch(struct signpost_result* result);

extern void
preempt_handle_stalled(struct signpost_result* result);

#define do_signpost(fn, result)                         \
    do {                                                \
        fn((result));                                   \
        if ((result)->mode == SWITCH_MODE_FAST) {       \
            thread_stats_update_leaving();              \
            return (result)->stack;                     \
        }                                               \
        if ((result)->mode == SWITCH_MODE_GIVEUP) {     \
            schedule();                                 \
            fail("unexpected return");                  \
        }                                               \
    } while (0)

ptr_t
switch_signposting()
{
    struct signpost_result result;

    do_signpost(preempt_handle_stalled, &result);

    do_signpost(signal_dispatch, &result);

    thread_stats_update_leaving();

    return 0;
}