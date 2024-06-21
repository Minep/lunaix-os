#ifndef __LUNAIX_ARCH_HART_H
#define __LUNAIX_ARCH_HART_H

#include "vectors.h"

#ifndef __ASM__
#include <lunaix/compiler.h>
#include <sys/cpu.h>

struct exec_param;

struct regcontext
{
    u32_t eax;
    u32_t ebx;
    u32_t ecx;
    u32_t edx;
    u32_t edi;
    u32_t ebp;
    u32_t esi;
    u32_t ds;
    u32_t es;
    u32_t fs;
    u32_t gs;
} compact;

struct hart_state
{
    unsigned int depth;
    struct regcontext registers;
    union
    {
        u32_t esp;
        volatile struct exec_param* execp;
    };
} compact;

struct exec_param
{
    struct hart_state* parent_state;
    u32_t vector;
    u32_t err_code;
    u32_t eip;
    u32_t cs;
    u32_t eflags;
    u32_t esp;
    u32_t ss;
} compact;


static inline void
hart_change_execution(struct hart_state* hstate, ptr_t pc, ptr_t sp)
{
    hstate->execp->eip = pc;
    hstate->execp->esp = sp;
}

static inline ptr_t
hart_pc(struct hart_state* hstate)
{
    return hstate->execp->eip;
}

static inline ptr_t
hart_sp(struct hart_state* hstate)
{
    return hstate->execp->esp;
}

static inline bool
kernel_context(struct hart_state* hstate)
{
    return !((hstate->execp->cs) & 0b11);
}

static inline ptr_t
hart_stack_frame(struct hart_state* hstate)
{
    return hstate->registers.ebp;
}

static inline int
hart_vector_stamp(struct hart_state* hstate) {
    return hstate->execp->vector;
}

static inline unsigned int
hart_ecause(struct hart_state* hstate) {
    return hstate->execp->err_code;
}

#endif

#endif /* __LUNAIX_ARCH_HART_H */
