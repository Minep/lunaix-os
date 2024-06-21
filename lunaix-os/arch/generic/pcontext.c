#include <lunaix/process.h>
#include <lunaix/pcontext.h>
#include <lunaix/mm/vmm.h>
#include <klibc/string.h>

bool
inject_transfer_context(ptr_t vm_mnt, struct transfer_context* tctx)
{
    return false;
}

void
thread_setup_trasnfer(struct transfer_context* tctx, 
                      ptr_t kstack_tp, ptr_t ustack_pt, 
                      ptr_t entry, bool to_user) 
{
    fail("unimplemented");
}