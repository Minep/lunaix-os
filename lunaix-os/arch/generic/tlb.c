#include <asm/tlb.h>
#include <lunaix/process.h>

_default void
tlb_flush_mm(struct proc_mm* mm, ptr_t addr)
{
    __tlb_flush_asid(procvm_asid(mm), addr);
}

_default void
tlb_flush_mm_range(struct proc_mm* mm, ptr_t addr, unsigned int npages)
{
    tlb_flush_asid_range(procvm_asid(mm), addr, npages);
}


_default void
tlb_flush_vmr(struct mm_region* vmr, ptr_t va)
{
    __tlb_flush_asid(procvm_asid(vmr->proc_vms), va);
}

_default void
tlb_flush_vmr_all(struct mm_region* vmr)
{
    tlb_flush_asid_range(procvm_asid(vmr->proc_vms), 
                            vmr->start, leaf_count(vmr->end - vmr->start));
}

_default void
tlb_flush_vmr_range(struct mm_region* vmr, ptr_t addr, unsigned int npages)
{
    tlb_flush_asid_range(procvm_asid(vmr->proc_vms), addr, npages);
}