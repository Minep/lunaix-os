#ifndef __LUNAIX_ARCH_HART_H
#define __LUNAIX_ARCH_HART_H

#include "vectors.h"

#ifndef __ASM__
#include <lunaix/compiler.h>
#include <sys/cpu.h>

struct exec_param;

struct regcontext
{
    reg_t eax;
    reg_t ebx;
    reg_t ecx;
    reg_t edx;
    reg_t edi;
    reg_t ebp;
    reg_t esi;
    reg_t ds;
    reg_t es;
    reg_t fs;
    reg_t gs;
} compact;

struct hart_state
{
    unsigned int depth;
    struct regcontext registers;
    union
    {
        reg_t esp;
        volatile struct exec_param* execp;
    };
} compact;

struct exec_param
{
    struct hart_state* parent_state;
    reg_t vector;
    reg_t err_code;
    reg_t eip;
    reg_t cs;
    reg_t eflags;
    reg_t esp;
    reg_t ss;
} compact;


static inline void
hart_flow_redirect(struct hart_state* hstate, ptr_t pc, ptr_t sp)
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
