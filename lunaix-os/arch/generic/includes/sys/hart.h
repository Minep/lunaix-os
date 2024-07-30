#ifndef __LUNAIX_ARCH_HART_H
#define __LUNAIX_ARCH_HART_H

#ifndef __ASM__
#include <lunaix/compiler.h>
#include <sys/cpu.h>

struct exec_param;

struct regcontext
{
    
} compact;

struct hart_state
{
    
} compact;

struct exec_param
{
    struct hart_state* parent_state;
} compact;

ptr_t
hart_pc(struct hart_state* state);

ptr_t
hart_sp(struct hart_state* state);

bool
kernel_context(struct hart_state* hstate);

ptr_t
hart_stack_frame(struct hart_state* hstate);

int
hart_vector_stamp(struct hart_state* hstate);

unsigned int
hart_ecause(struct hart_state* hstate);

struct hart_state*
hart_parent_state(struct hart_state* hstate);

void
hart_push_state(struct hart_state* p_hstate, struct hart_state* hstate);

#endif

#endif /* __LUNAIX_ARCH_HART_H */
