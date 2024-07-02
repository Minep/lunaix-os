#ifndef __LUNAIX_HART64_DEF_H
#define __LUNAIX_HART64_DEF_H
#ifdef CONFIG_ARCH_X86_64
#include <lunaix/types.h>

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


static inline void
hart_flow_redirect(struct hart_state* hstate, ptr_t pc, ptr_t sp)
{
    hstate->execp->rip = pc;
    hstate->execp->rsp = sp;
}

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

#endif

#endif /* __LUNAIX_HART64_DEF_H */
