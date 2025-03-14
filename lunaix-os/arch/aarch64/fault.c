#include <lunaix/mm/fault.h>
#include <asm/aa64_exception.h>
#include <asm/aa64_msrs.h>
#include <asm/hart.h>

bool
__arch_prepare_fault_context(struct fault_context* fault)
{
    struct hart_state* hs = fault->hstate;

    ptr_t ptr = read_sysreg(FAR_EL1);
    if (!ptr) {
        return false;
    }

    fault->fault_ptep  = mkptep_va(VMS_SELF, ptr);
    fault->fault_data  = esr_ec(hs->execp.syndrome);
    fault->fault_instn = hart_pc(hs);
    fault->fault_va    = ptr;

    return true;
}

void 
handle_mm_abort(struct hart_state* state)
{
    struct fault_context fault;

    __arch_prepare_fault_context(&fault);

    if (!handle_page_fault(&fault)) {
        fault_resolving_failed(&fault);
    }

    return;
}