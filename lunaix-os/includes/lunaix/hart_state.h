#ifndef __LUNAIX_CONTEXT_H
#define __LUNAIX_CONTEXT_H

struct exec_param;
struct regcontext;
struct hart_state;

#include <lunaix/compiler.h>
#include <asm/hart.h>

struct hart_transition 
{
    ptr_t inject;
    struct {
        struct hart_state state;
        struct exec_param eret;
    } transfer;
};

bool
install_hart_transition(ptr_t vm_mnt, struct hart_transition* tctx);

void
hart_prepare_transition(struct hart_transition* tctx, 
                      ptr_t kstack_tp, ptr_t ustack_pt, 
                      ptr_t entry, bool to_user);

static inline void
hart_user_transfer(struct hart_transition* tctx, 
                            ptr_t kstack_tp, ptr_t ustack_pt, 
                            ptr_t entry) 
{
    hart_prepare_transition(tctx, kstack_tp, ustack_pt, entry, true);
}

static inline void
hart_kernel_transfer(struct hart_transition* tctx, 
                            ptr_t kstack_tp,  ptr_t entry) 
{
    hart_prepare_transition(tctx, kstack_tp, 0, entry, false);
}

#endif /* __LUNAIX_CONTEXT_H */
