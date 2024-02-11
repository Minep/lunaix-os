#include <klibc/string.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
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
vmm_alloc_page(pte_t* ptep, pte_t pte)
{
    ptr_t pa = pmm_alloc_page(PP_FGPERSIST);
    if (!pa) {
        return null_pte;
    }

    pte = pte_setpaddr(pte, pa);
    pte = pte_mkloaded(pte);
    set_pte(ptep, pte);

    pte_t* ptep_next = __LnTEP_SHIFT_NEXT(ptep);
    cpu_flush_page((ptr_t)ptep_next);

    memset(ptep_next, 0, LFT_SIZE);

    return pte;
}

int
vmm_set_mapping(ptr_t mnt, ptr_t va, ptr_t pa, pte_attr_t prot)
{
    assert(!va_offset(va));

    pte_t* ptep = mkptep_va(mnt, va);
    pte_t  pte  = mkpte(pa, prot);

    set_pte(ptep, pte);

    return 1;
}

ptr_t
vmm_del_mapping(ptr_t mnt, ptr_t va)
{
    assert(!va_offset(va));

    pte_t* ptep = mkptep_va(mnt, va);

    pte_t old = *ptep;

    set_pte(ptep, null_pte);

    return pte_paddr(old);
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
vmm_v2p(ptr_t va)
{
    u32_t l1_index = L1_INDEX(va);
    u32_t l2_index = L2_INDEX(va);

    x86_page_table* l1pt = (x86_page_table*)L1_BASE_VADDR;
    x86_pte_t l1pte = l1pt->entry[l1_index];

    if (l1pte) {
        x86_pte_t* l2pte =
          &((x86_page_table*)L2_VADDR(l1_index))->entry[l2_index];

        if (l2pte) {
            return PG_ENTRY_ADDR(*l2pte) | ((ptr_t)va & 0xfff);
        }
    }
    return 0;
}

ptr_t
vmm_v2pat(ptr_t mnt, ptr_t va)
{
    u32_t l1_index = L1_INDEX(va);
    u32_t l2_index = L2_INDEX(va);

    x86_page_table* l1pt = (x86_page_table*)(mnt | 1023 << 12);
    x86_pte_t l1pte = l1pt->entry[l1_index];

    if (l1pte) {
        x86_pte_t* l2pte =
          &((x86_page_table*)(mnt | (l1_index << 12)))->entry[l2_index];

        if (l2pte) {
            return PG_ENTRY_ADDR(*l2pte) | ((ptr_t)va & 0xfff);
        }
    }
    return 0;
}

ptr_t
vms_mount(ptr_t mnt, ptr_t vms_root)
{
    assert(vms_root);

    pte_t* ptep = mkl0tep_va(VMS_SELF, mnt);
    set_pte(ptep, mkpte(vms_root, KERNEL_DATA));
    cpu_flush_page(mnt);
    return mnt;
}

ptr_t
vms_unmount(ptr_t mnt)
{
    pte_t* ptep = mkl0tep_va(VMS_SELF, mnt);
    set_pte(ptep, null_pte);
    cpu_flush_page(mnt);
    return mnt;
}

ptr_t
vmm_dup_page(ptr_t pa)
{
    // FIXME use latest vmm api
    ptr_t new_ppg = pmm_alloc_page(0);
    vmm_set_mapping(VMS_SELF, PG_MOUNT_3, new_ppg, KERNEL_DATA);
    vmm_set_mapping(VMS_SELF, PG_MOUNT_4, pa, KERNEL_DATA);

    // FIXME Arch-dependent feature
    asm volatile("movl %1, %%edi\n"
                 "movl %2, %%esi\n"
                 "rep movsl\n" ::"c"(1024),
                 "r"(PG_MOUNT_3),
                 "r"(PG_MOUNT_4)
                 : "memory", "%edi", "%esi");

    vmm_del_mapping(VMS_SELF, PG_MOUNT_3);
    vmm_del_mapping(VMS_SELF, PG_MOUNT_4);

    return new_ppg;
}