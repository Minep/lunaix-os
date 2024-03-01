#include <klibc/string.h>
#include <lunaix/mm/page.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#include <sys/cpu.h>
#include <sys/mm/mm_defs.h>

LOG_MODULE("VM")

void
vmm_init()
{
    // XXX: something here?
}

pte_t
vmm_tryptep(pte_t* ptep, size_t lvl_size)
{
    ptr_t va = ptep_va(ptep, lvl_size);
    pte_t* _ptep = mkl0tep(ptep);
    pte_t pte;

    if (pte_isnull(pte = *_ptep) || _ptep == ptep) 
        return pte;

#if LnT_ENABLED(1)
    _ptep = getl1tep(_ptep, va);
    if (_ptep == ptep || pte_isnull(pte = *_ptep)) 
        return pte;
#endif
#if LnT_ENABLED(2)
    _ptep = getl2tep(_ptep, va);
    if (_ptep == ptep || pte_isnull(pte = *_ptep)) 
        return pte;
#endif
#if LnT_ENABLED(3)
    _ptep = getl3tep(_ptep, va);
    if (_ptep == ptep || pte_isnull(pte = *_ptep)) 
        return pte;
#endif
    _ptep = getlftep(_ptep, va);
    return *_ptep;
}

ptr_t
vms_mount(ptr_t mnt, ptr_t vms_root)
{
    assert(vms_root);

    pte_t* ptep = mkl0tep_va(VMS_SELF, mnt);
    set_pte(ptep, mkpte(vms_root, KERNEL_DATA));
    tlb_flush_kernel(mnt);
    return mnt;
}

ptr_t
vms_unmount(ptr_t mnt)
{
    pte_t* ptep = mkl0tep_va(VMS_SELF, mnt);
    set_pte(ptep, null_pte);
    tlb_flush_kernel(mnt);
    return mnt;
}


void
ptep_alloc_hierarchy(pte_t* ptep, ptr_t va, pte_attr_t prot)
{
    pte_t* _ptep;
    
    _ptep = mkl0tep(ptep);
    if (_ptep == ptep) {
        return;
    }

    _ptep = mkl1t(_ptep, va, prot);
    if (_ptep == ptep) {
        return;
    }

    _ptep = mkl2t(_ptep, va, prot);
    if (_ptep == ptep) {
        return;
    }

    _ptep = mkl3t(_ptep, va, prot);
    if (_ptep == ptep) {
        return;
    }

    _ptep = mklft(_ptep, va, prot);
    assert(_ptep == ptep);
}