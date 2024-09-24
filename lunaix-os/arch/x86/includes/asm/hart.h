#ifndef __LUNAIX_ARCH_HART_H
#define __LUNAIX_ARCH_HART_H

#include "x86_ivs.h"

#ifndef __ASM__
#include <lunaix/types.h>

#include "x86_cpu.h"

struct hart_state;

#ifdef CONFIG_ARCH_X86_64
struct regcontext
{
    reg_t rax;
    reg_t rbx;
    reg_t rcx;
    reg_t rdx;
    reg_t rdi;
    reg_t rbp;
    reg_t rsi;
    reg_t r8;
    reg_t r9;
    reg_t r10;
    reg_t r11;
    reg_t r12;
    reg_t r13;
    reg_t r14;
    reg_t r15;
} compact;

struct exec_param
{
    struct hart_state* parent_state;
    reg_t vector;
    reg_t err_code;
    reg_t rip;
    reg_t cs;
    reg_t rflags;
    reg_t rsp;
    reg_t ss;
} compact;


#else
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

#endif


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


#ifdef CONFIG_ARCH_X86_64
static inline ptr_t
hart_pc(struct hart_state* hstate)
{
    return hstate->execp->rip;
}

static inline ptr_t
hart_sp(struct hart_state* hstate)
{
    return hstate->execp->rsp;
}

static inline bool
kernel_context(struct hart_state* hstate)
{
    return !((hstate->execp->cs) & 0b11);
}

static inline ptr_t
hart_stack_frame(struct hart_state* hstate)
{
    return hstate->registers.rbp;
}

#else
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

#endif

#endif

#endif /* __LUNAIX_ARCH_HART_H */
