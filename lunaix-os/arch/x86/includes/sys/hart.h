#ifndef __LUNAIX_ARCH_HART_H
#define __LUNAIX_ARCH_HART_H

#include "vectors.h"

#ifndef __ASM__
#include <lunaix/compiler.h>
#include <sys/cpu.h>

struct exec_param;
struct regcontext;

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

#ifdef CONFIG_ARCH_X86_64
#include "hart64_def.h"
#else
#include "hart32_def.h"
#endif

static inline int
hart_vector_stamp(struct hart_state* hstate) {
    return hstate->execp->vector;
}

static inline unsigned int
hart_ecause(struct hart_state* hstate) {
    return hstate->execp->err_code;
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


#endif

#endif /* __LUNAIX_ARCH_HART_H */
