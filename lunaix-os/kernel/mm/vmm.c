#include <klibc/string.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <sys/cpu.h>

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
      (x86_page_table*)pmm_alloc_page(KERNEL_PID, PP_FGPERSIST);
    for (size_t i = 0; i < PG_MAX_ENTRIES; i++) {
        dir->entry[i] = PTE_NULL;
    }

    // 递归映射，方便我们在软件层面进行查表地址转换
    dir->entry[PG_MAX_ENTRIES - 1] = NEW_L1_ENTRY(T_SELF_REF_PERM, dir);

    return dir;
}

int
vmm_set_mapping(ptr_t mnt, ptr_t va, ptr_t pa, pt_attr attr, int options)
{
    assert((ptr_t)va % PG_SIZE == 0);

    ptr_t l1_inx = L1_INDEX(va);
    ptr_t l2_inx = L2_INDEX(va);
    x86_page_table* l1pt = (x86_page_table*)(mnt | (1023 << 12));
    x86_page_table* l2pt = (x86_page_table*)(mnt | (l1_inx << 12));

    // See if attr make sense
    assert(attr <= 128);

    x86_pte_t* l1pte = &l1pt->entry[l1_inx];
    if (!*l1pte) {
        x86_page_table* new_l1pt_pa =
          (x86_page_table*)pmm_alloc_page(KERNEL_PID, PP_FGPERSIST);

        // 物理内存已满！
        if (!new_l1pt_pa) {
            return 0;
        }

        // This must be writable
        *l1pte = NEW_L1_ENTRY(attr | PG_WRITE | PG_PRESENT, new_l1pt_pa);

        // make sure our new l2 table is visible to CPU
        cpu_flush_page((ptr_t)l2pt);

        memset((void*)l2pt, 0, PG_SIZE);
    } else {
        if ((attr & PG_ALLOW_USER) && !(*l1pte & PG_ALLOW_USER)) {
            *l1pte |= PG_ALLOW_USER;
        }

        x86_pte_t pte = l2pt->entry[l2_inx];
        if (pte && (options & VMAP_IGNORE)) {
            return 1;
        }
    }

    if (mnt == VMS_SELF) {
        cpu_flush_page(va);
    }

    if ((options & VMAP_NOMAP)) {
        return 1;
    }

    l2pt->entry[l2_inx] = NEW_L2_ENTRY(attr, pa);
    return 1;
}

ptr_t
vmm_del_mapping(ptr_t mnt, ptr_t va)
{
    assert(((ptr_t)va & 0xFFFU) == 0);

    u32_t l1_index = L1_INDEX(va);
    u32_t l2_index = L2_INDEX(va);

    // prevent unmap of recursive mapping region
    if (l1_index == 1023) {
        return 0;
    }

    x86_page_table* l1pt = (x86_page_table*)(mnt | (1023 << 12));

    x86_pte_t l1pte = l1pt->entry[l1_index];

    if (l1pte) {
        x86_page_table* l2pt = (x86_page_table*)(mnt | (l1_index << 12));
        x86_pte_t l2pte = l2pt->entry[l2_index];

        cpu_flush_page(va);
        l2pt->entry[l2_index] = PTE_NULL;

        return PG_ENTRY_ADDR(l2pte);
    }

    return 0;
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