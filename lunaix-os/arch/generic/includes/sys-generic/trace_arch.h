#ifndef __LUNAIX_TRACE_ARCH_H
#define __LUNAIX_TRACE_ARCH_H

#include <lunaix/hart_state.h>

void
trace_print_transistion_short(struct hart_state* hstate);

void
trace_print_transition_full(struct hart_state* hstate);

void
trace_dump_state(struct hart_state* hstate);
#endif /* __LUNAIX_TRACE_ARCH_H */
