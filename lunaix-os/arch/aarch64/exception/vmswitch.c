#include <lunaix/process.h>
#include <asm/aa64_mmu.h>
#include <asm/tlb.h>

void
aa64_switch_ttbr()
{
    ptr_t ttbr;
    struct proc_mm* vms;

    vms = proc_vmroot();

    ttbr = read_sysreg(TTBR0_EL1);

    /*
        We don't differentiate ASID for now
        and CnP=1
    */

    if (!BITS_GET(ttbr, TTBR_BADDR) == vms->vmroot) {
        return;
    }

    BITS_SET(ttbr, TTBR_BADDR, vms->vmroot);

    set_sysreg(TTBR0_EL1, ttbr);
    
    /*
        TODO a more fine grain control of flushing
        Unlike x86, hardware will not flush TLB upon switching
        the translation base.

        as kernel address space are shared, flushing should be avoided, 
        thus the apporachs:
            1. enable the use of ASID and flush accordingly
            2. enable the use of ASID and TTBR1 to house kernel, 
               use TCR_EL1.A1 to switch between ASIDs in TTBR0 and TTBR1.
            3. range flushing (RVAAE1) on all memory regions used by user space.

    */
    __tlb_flush_all();
}