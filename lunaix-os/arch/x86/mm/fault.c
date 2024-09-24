#include <lunaix/mm/fault.h>
#include <lunaix/hart_state.h>

bool
__arch_prepare_fault_context(struct fault_context* fault)
{
    struct hart_state* ictx = fault->hstate;

    ptr_t ptr = cpu_ldeaddr();
    if (!ptr) {
        return false;
    }

    fault->fault_ptep  = mkptep_va(VMS_SELF, ptr);
    fault->fault_data  = ictx->execp->err_code;
    fault->fault_instn = hart_pc(ictx);
    fault->fault_va    = ptr;

    return true;
}


void
intr_routine_page_fault(const struct hart_state* hstate)
{
    if (hstate->depth > 10) {
        // Too many nested fault! we must messed up something
        // XXX should we failed silently?
        spin();
    }

    struct fault_context fault = { .hstate = hstate };

    if (!__arch_prepare_fault_context(&fault)) {
        goto failed;
    }

    if (!handle_page_fault(&fault)) {
        goto failed;
    }

    return;

failed:
    fault_resolving_failed(&fault);
}