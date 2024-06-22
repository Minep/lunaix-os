#include <lunaix/mm/fault.h>
#include <lunaix/mm/region.h>
#include <lunaix/process.h>
#include <lunaix/hart_state.h>

#include <sys/mm/mm_defs.h>

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
    fault->fault_instn = ictx->execp->eip;
    fault->fault_va    = ptr;

    return true;
}