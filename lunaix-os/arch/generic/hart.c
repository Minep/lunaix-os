#include <lunaix/process.h>
#include <lunaix/hart_state.h>
#include <lunaix/mm/vmm.h>
#include <klibc/string.h>

bool
install_hart_transition(ptr_t vm_mnt, struct hart_transition* tctx)
{
    return false;
}

void
hart_prepare_transition(struct hart_transition* tctx, 
                      ptr_t kstack_tp, ptr_t ustack_pt, 
                      ptr_t entry, bool to_user) 
{
    fail("unimplemented");
}