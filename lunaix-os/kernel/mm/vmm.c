#include <klibc/string.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#include <sys/cpu.h>
#include <sys/mm/mm_defs.h>

LOG_MODULE("VMM")

void
vmm_init()
{
    // XXX: something here?
}

x86_page_table*
vmm_init_pd()
{
    x86_page_table* dir =
      (x86_page_table*)pmm_alloc_page(PP_FGPERSIST);
    for (size_t i = 0; i < PG_MAX_ENTRIES; i++) {
        dir->entry[i] = PTE_NULL;
    }

    // 递归映射，方便我们在软件层面进行查表地址转换
    dir->entry[PG_MAX_ENTRIES - 1] = NEW_L1_ENTRY(T_SELF_REF_PERM, dir);

    return dir;
}

ptr_t 
pagetable_alloc_one()
{
    ptr_t pa = pmm_alloc_page(PP_FGPERSIST);
    if (pa) {
        memset((void*)pa, 0, LFT_SIZE);
    }

    return pa;
}

int
vmm_set_mapping(ptr_t mnt, ptr_t va, ptr_t pa, pte_attr_t prot)
{
    assert(!va_offset(va));

    pte_t* ptep = mkptep_va(mnt, va);
    pte_t  pte  = mkpte(pa, prot);

    vmm_set_pte_at(ptep, pte_mkleaf(pte), LFT_SIZE);

    return 1;
}

void 
vmm_set_pte_at(pte_t* ptep, pte_t pte, size_t lvl_size) 
{
    ptr_t va = page_addr(ptep_vm_pfn(ptep));
    pte_t* ptep_ = mkl1tep(ptep);

    if (lvl_size <= L1T_SIZE) {
        assert(ptep_ = mkl1t(ptep_, va));
    }

    if (lvl_size <= L2T_SIZE) {
        assert(ptep_ = mkl2t(ptep_, va));
    }

    if (lvl_size <= L3T_SIZE) {
        assert(ptep_ = mkl3t(ptep_, va));
    }

    if (lvl_size <= LFT_SIZE) {
        assert(ptep_ = mklft(ptep_, va));
    }

    // We should arrive with the same ptep.
    assert(ptep_ == ptep);

    pte_write_entry(ptep_, pte);
    
    if (ptep_vm_mnt(ptep_) == VMS_SELF) {
        cpu_flush_page(va);
    }
}

ptr_t
vmm_del_mapping(ptr_t mnt, ptr_t va)
{
    assert(!va_offset(va));

    pte_t* ptep = mkptep_va(mnt, va);

    pte_t old = *ptep;

    vmm_set_pte(ptep, mkpte_raw(0));

    return pte_paddr(old);
}

int
vmm_lookup(ptr_t va, v_mapping* mapping)
{
    return vmm_lookupat(VMS_SELF, va, mapping);
}

int
vmm_lookupat(ptr_t mnt, ptr_t va, v_mapping* mapping)
{
    u32_t l1_index = L1_INDEX(va);
    u32_t l2_index = L2_INDEX(va);

    x86_page_table* l1pt = (x86_page_table*)(mnt | 1023 << 12);
    x86_pte_t l1pte = l1pt->entry[l1_index];

    if (l1pte) {
        x86_pte_t* l2pte =
          &((x86_page_table*)(mnt | (l1_index << 12)))->entry[l2_index];

        if (l2pte) {
            mapping->flags = PG_ENTRY_FLAGS(*l2pte);
            mapping->pa = PG_ENTRY_ADDR(*l2pte);
            mapping->pn = mapping->pa >> PG_SIZE_BITS;
            mapping->pte = l2pte;
            mapping->va = va;
            return 1;
        }
    }

    return 0;
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
vmm_mount_pd(ptr_t mnt, ptr_t pde)
{
    assert(pde);

    x86_page_table* l1pt = (x86_page_table*)L1_BASE_VADDR;
    l1pt->entry[(mnt >> 22)] = NEW_L1_ENTRY(T_SELF_REF_PERM, pde);
    cpu_flush_page(mnt);
    return mnt;
}

ptr_t
vmm_unmount_pd(ptr_t mnt)
{
    x86_page_table* l1pt = (x86_page_table*)L1_BASE_VADDR;
    l1pt->entry[(mnt >> 22)] = 0;
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