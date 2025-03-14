#ifndef __LUNAIX_TRACE_ARCH_H
#define __LUNAIX_TRACE_ARCH_H

#include <lunaix/hart_state.h>

static inline bool 
arch_valid_fp(ptr_t ptr) {
    extern int __bsskstack_end[];
    extern int __bsskstack_start[];
    return ((ptr_t)__bsskstack_start <= ptr && ptr <= (ptr_t)__bsskstack_end);
}

void
trace_print_transistion_short(struct hart_state* hstate);

void
trace_print_transition_full(struct hart_state* hstate);

void
trace_dump_state(struct hart_state* hstate);
#endif /* __LUNAIX_TRACE_ARCH_H */
