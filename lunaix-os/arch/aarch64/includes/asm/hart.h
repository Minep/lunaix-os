#ifndef __LUNAIX_ARCH_HART_H
#define __LUNAIX_ARCH_HART_H

#ifndef __ASM__
#include <lunaix/types.h>
#include <lunaix/bits.h>
#include <asm/aa64_spsr.h>

#define SYNDROME_ETYPE  BITFIELD(63, 56)

struct hart_state;

struct regcontext
{
    union {
        reg_t x[31];
        struct {
            reg_t x_[29];
            reg_t fp;
            reg_t lr;
        };
    };
} compact align(8);

struct exec_param
{
    struct hart_state* parent_state;
    reg_t spsr;
    reg_t link;
    struct {
        reg_t sp_el0;
        reg_t rsvd;
    };
    
    reg_t syndrome;
} compact align(8);

struct hart_state
{
    reg_t depth;
    struct regcontext registers;
    struct exec_param execp;
} compact align(16);

static inline int
hart_vector_stamp(struct hart_state* hstate) 
{
    return BITS_GET(hstate->execp.syndrome, SYNDROME_ETYPE);
}

static inline unsigned int
hart_ecause(struct hart_state* hstate) 
{
    return hstate->execp.syndrome;
}

static inline struct hart_state*
hart_parent_state(struct hart_state* hstate)
{
    return hstate->execp.parent_state;
}

static inline void
hart_push_state(struct hart_state* p_hstate, struct hart_state* hstate)
{
    hstate->execp.parent_state = p_hstate;
}

static inline ptr_t
hart_pc(struct hart_state* hstate)
{
    return hstate->execp.link;
}

static inline ptr_t
hart_sp(struct hart_state* hstate)
{
    return __ptr(&hstate[-1]);
}

static inline bool
kernel_context(struct hart_state* hstate)
{
    reg_t spsr;

    spsr = hstate->execp.spsr;
    return !spsr_from_el0(spsr);
}

static inline ptr_t
hart_stack_frame(struct hart_state* hstate)
{
    return hstate->registers.fp;
}

#endif

#endif /* __LUNAIX_ARCH_HART_H */
