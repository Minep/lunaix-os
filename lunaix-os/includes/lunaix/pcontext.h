#ifndef __LUNAIX_CONTEXT_H
#define __LUNAIX_CONTEXT_H

struct exec_param;
struct regcontext;
struct pcontext;
typedef struct pcontext isr_param;

#include <lunaix/compiler.h>
#include <sys/interrupts.h>

struct transfer_context 
{
    ptr_t inject;
    struct {
        struct pcontext isr;
        struct exec_param eret;
    } compact transfer;
};

int
inject_transfer_context(ptr_t vm_mnt, struct transfer_context* tctx);

void
thread_setup_trasnfer(struct transfer_context* tctx, 
                      ptr_t kstack_tp, ptr_t ustack_pt, 
                      ptr_t entry, bool to_user);

static void inline
thread_create_user_transfer(struct transfer_context* tctx, 
                            ptr_t kstack_tp, ptr_t ustack_pt, 
                            ptr_t entry) 
{
    thread_setup_trasnfer(tctx, kstack_tp, ustack_pt, entry, true);
}

static void inline
thread_create_kernel_transfer(struct transfer_context* tctx, 
                            ptr_t kstack_tp,  ptr_t entry) 
{
    thread_setup_trasnfer(tctx, kstack_tp, 0, entry, false);
}

#endif /* __LUNAIX_CONTEXT_H */
