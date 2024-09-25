#ifndef __LUNAIX_ARCH_HART_H
#define __LUNAIX_ARCH_HART_H

#ifndef __ASM__
#include <lunaix/types.h>

struct hart_state;

struct regcontext
{
    union {
        reg_t x[32];
        struct {
            reg_t x[29];
            reg_t fp;
            reg_t lr;
            reg_t sp;
        };
    };
} compact;

struct exec_param
{
    struct hart_state* parent_state;
    reg_t vector;
    reg_t syndrome;
    reg_t elink;
    reg_t sp;
} compact;

struct hart_state
{
    reg_t depth;
    struct regcontext registers;
    union
    {
        reg_t sp;
        volatile struct exec_param* execp;
    };
} compact;

static inline int
hart_vector_stamp(struct hart_state* hstate) {
    return hstate->execp->vector;
}

static inline unsigned int
hart_ecause(struct hart_state* hstate) {
    return hstate->execp->syndrome;
}

static inline struct hart_state*
hart_parent_state(struct hart_state* hstate)
{
    return hstate->execp->parent_state;
}

static inline void
hart_push_state(struct hart_state* p_hstate, struct hart_state* hstate)
{
    hstate->execp->parent_state = p_hstate;
}

static inline ptr_t
hart_pc(struct hart_state* hstate)
{
    return hstate->execp->elink;
}

static inline ptr_t
hart_sp(struct hart_state* hstate)
{
    return hstate->execp->sp;
}

static inline bool
kernel_context(struct hart_state* hstate)
{
    // TODO
    return false;
}

static inline ptr_t
hart_stack_frame(struct hart_state* hstate)
{
    return hstate->registers.fp;
}

#endif

#endif /* __LUNAIX_ARCH_HART_H */
