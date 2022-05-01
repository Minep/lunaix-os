#include <hal/cpu.h>
#include <klibc/string.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

#include <stdbool.h>

void
vmm_init()
{
    // XXX: something here?
}

x86_page_table*
vmm_init_pd()
{
    x86_page_table* dir = (x86_page_table*)pmm_alloc_page();
    for (size_t i = 0; i < PG_MAX_ENTRIES; i++) {
        dir->entry[i] = PTE_NULL;
    }

    // 递归映射，方便我们在软件层面进行查表地址转换
    dir->entry[PG_MAX_ENTRIES - 1] = NEW_L1_ENTRY(T_SELF_REF_PERM, dir);

    return dir;
}

int
__vmm_map_internal(uint32_t l1_inx,
                   uint32_t l2_inx,
                   uintptr_t pa,
                   pt_attr attr,
                   int forced)
{
    x86_page_table* l1pt = (x86_page_table*)L1_BASE_VADDR;
    x86_page_table* l2pt = (x86_page_table*)L2_VADDR(l1_inx);

    // See if attr make sense
    assert(attr <= 128);

    if (!l1pt->entry[l1_inx]) {
        x86_page_table* new_l1pt_pa = pmm_alloc_page();

        // 物理内存已满！
        if (!new_l1pt_pa) {
            return 0;
        }

        l1pt->entry[l1_inx] = NEW_L1_ENTRY(attr, new_l1pt_pa);
        memset((void*)L2_VADDR(l1_inx), 0, PG_SIZE);
    }

    x86_pte_t l2pte = l2pt->entry[l2_inx];
    if (l2pte) {
        if (!forced) {
            return 0;
        }
        if (HAS_FLAGS(l2pte, PG_PRESENT)) {
            assert_msg(pmm_free_page(GET_PG_ADDR(l2pte)), "fail to release physical page");
        }
    }

    l2pt->entry[l2_inx] = NEW_L2_ENTRY(attr, pa);

    return 1;
}

void*
vmm_map_page(void* va, void* pa, pt_attr tattr)
{
    // 显然，对空指针进行映射没有意义。
    if (!pa || !va) {
        return NULL;
    }

    assert(((uintptr_t)va & 0xFFFU) == 0) assert(((uintptr_t)pa & 0xFFFU) == 0);

    uint32_t l1_index = L1_INDEX(va);
    uint32_t l2_index = L2_INDEX(va);
    x86_page_table* l1pt = (x86_page_table*)L1_BASE_VADDR;

    // 在页表与页目录中找到一个可用的空位进行映射（位于va或其附近）
    x86_pte_t l1pte = l1pt->entry[l1_index];
    x86_page_table* l2pt = (x86_page_table*)L2_VADDR(l1_index);
    while (l1pte && l1_index < PG_MAX_ENTRIES) {
        if (l2_index == PG_MAX_ENTRIES) {
            l1_index++;
            l2_index = 0;
            l1pte = l1pt->entry[l1_index];
            l2pt = (x86_page_table*)L2_VADDR(l1_index);
        }
        // 页表有空位，只需要开辟一个新的 PTE (Level 2)
        if (l2pt && !l2pt->entry[l2_index]) {
            l2pt->entry[l2_index] = NEW_L2_ENTRY(tattr, pa);
            return (void*)V_ADDR(l1_index, l2_index, PG_OFFSET(va));
        }
        l2_index++;
    }

    // 页目录与所有页表已满！
    if (l1_index > PG_MAX_ENTRIES) {
        return NULL;
    }

    if (!__vmm_map_internal(l1_index, l2_index, (uintptr_t)pa, tattr, false)) {
        return NULL;
    }

    return (void*)V_ADDR(l1_index, l2_index, PG_OFFSET(va));
}

void*
vmm_fmap_page(void* va, void* pa, pt_attr tattr)
{
    if (!pa || !va) {
        return NULL;
    }

    assert(((uintptr_t)va & 0xFFFU) == 0) assert(((uintptr_t)pa & 0xFFFU) == 0);

    uint32_t l1_index = L1_INDEX(va);
    uint32_t l2_index = L2_INDEX(va);

    if (!__vmm_map_internal(l1_index, l2_index, (uintptr_t)pa, tattr, true)) {
        return NULL;
    }

    cpu_invplg(va);

    return (void*)V_ADDR(l1_index, l2_index, PG_OFFSET(va));
}

void*
vmm_alloc_page(void* vpn, pt_attr tattr)
{
    void* pp = pmm_alloc_page();
    void* result = vmm_map_page(vpn, pp, tattr);
    if (!result) {
        pmm_free_page(pp);
    }
    return result;
}

int
vmm_alloc_pages(void* va, size_t sz, pt_attr tattr)
{
    assert((uintptr_t)va % PG_SIZE == 0) assert(sz % PG_SIZE == 0);

    void* va_ = va;
    for (size_t i = 0; i < (sz >> PG_SIZE_BITS); i++, va_ += PG_SIZE) {
        void* pp = pmm_alloc_page();
        uint32_t l1_index = L1_INDEX(va_);
        uint32_t l2_index = L2_INDEX(va_);
        if (!pp || !__vmm_map_internal(
                     l1_index, l2_index, (uintptr_t)pp, tattr, false)) {
            // if one failed, release previous allocated pages.
            va_ = va;
            for (size_t j = 0; j < i; j++, va_ += PG_SIZE) {
                vmm_unmap_page(va_);
            }

            return false;
        }
    }

    return true;
}

void
vmm_set_mapping(void* va, void* pa, pt_attr attr) {
    assert(((uintptr_t)va & 0xFFFU) == 0);

    uint32_t l1_index = L1_INDEX(va);
    uint32_t l2_index = L2_INDEX(va);

    // prevent map of recursive mapping region
    if (l1_index == 1023) {
        return;
    }
    
    __vmm_map_internal(l1_index, l2_index, (uintptr_t)pa, attr, false);
}

void
vmm_unmap_page(void* va)
{
    assert(((uintptr_t)va & 0xFFFU) == 0);

    uint32_t l1_index = L1_INDEX(va);
    uint32_t l2_index = L2_INDEX(va);

    // prevent unmap of recursive mapping region
    if (l1_index == 1023) {
        return;
    }

    x86_page_table* l1pt = (x86_page_table*)L1_BASE_VADDR;

    x86_pte_t l1pte = l1pt->entry[l1_index];

    if (l1pte) {
        x86_page_table* l2pt = (x86_page_table*)L2_VADDR(l1_index);
        x86_pte_t l2pte = l2pt->entry[l2_index];
        if (IS_CACHED(l2pte)) {
            pmm_free_page((void*)l2pte);
        }
        cpu_invplg(va);
        l2pt->entry[l2_index] = PTE_NULL;
    }
}

v_mapping
vmm_lookup(void* va)
{
    assert(((uintptr_t)va & 0xFFFU) == 0);

    uint32_t l1_index = L1_INDEX(va);
    uint32_t l2_index = L2_INDEX(va);

    x86_page_table* l1pt = (x86_page_table*)L1_BASE_VADDR;
    x86_pte_t l1pte = l1pt->entry[l1_index];

    v_mapping mapping = { .flags = 0, .pa = 0, .pn = 0 };
    if (l1pte) {
        x86_pte_t l2pte =
          ((x86_page_table*)L2_VADDR(l1_index))->entry[l2_index];
        if (l2pte) {
            mapping.flags = PG_ENTRY_FLAGS(l2pte);
            mapping.pa = PG_ENTRY_ADDR(l2pte);
            mapping.pn = mapping.pa >> PG_SIZE_BITS;
        }
    }

    return mapping;
}

void*
vmm_v2p(void* va)
{
    return (void*)vmm_lookup(va).pa;
}