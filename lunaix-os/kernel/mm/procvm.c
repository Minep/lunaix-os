#include "asm/pagetable.h"
#include "asm/tlb.h"
#include <lunaix/mm/procvm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/vastm.h>
#include <lunaix/process.h>
#include <lunaix/status.h>

#include <asm/mm_defs.h>

#include <klibc/string.h>

struct __vmcpy
{
    int err;
    struct mm_region* vmr;
};

static inline void
copy_leaf(struct __vmcpy* state, pte_t* dest, pte_t* src, pte_t pte)
{
    struct leaflet* leaflet;

    assert(pte_isnull(pte_at(dest)));

    if (shared_readonly_region(state->vmr)) 
    {
        pte = pte_mkwprotect(pte);
        set_pte(src, pte);
    } else {
        // do not copy the private page;
        return;
    }

    leaflet = pte_leaflet(pte);
    assert(leaflet_refcount(leaflet));

    set_pte(dest, pte);   
    leaflet_borrow(leaflet);
}


static inline pte_t*
copy_root(struct __vmcpy* state, pte_t* dest, pte_t* src, pte_t pte)
{
    struct leaflet* page;

    if (!pte_isnull(pte_at(dest)))
        return (pte_t*)pte_page_va(pte_at(dest));

    page = alloc_page_table();
    if (!page) {
        state->err = ENOMEM;
    }

    set_pte(dest, pte_setpaddr(pte, leaflet_addr(page)));

    return (pte_t*)leaflet_va(page);
}

static inline void
__copy_lft(struct __vmcpy* state, pte_t* dest, pte_t* src, 
        ptr_t va_from, ptr_t va_to)
{
    pte_t src_pte, *next;
    
    int i = level_index(va_from, LFT);
    dest += i;
    src += i;

    while (!state->err && va_from < va_to && i < LFT_LENGTH) 
    {
        src_pte = pte_at(src);

        if (!pte_isloaded(src_pte))
            goto cont;

        copy_leaf(state, dest, src, src_pte);
cont:
        i++;
        dest++;
        src++;
        va_from = (va_from + LFT_SIZE) & LFT_MASK;
    }
}

static inline void
__copy_l3t(struct __vmcpy* state, pte_t* dest, pte_t* src, 
        ptr_t va_from, ptr_t va_to)
{
#if has_ptlevel(L3T)
    pte_t src_pte, *next;
    
    int i = level_index(va_from, L3T);
    dest += i;
    src += i;

    while (!state->err && va_from < va_to && i < L3T_LENGTH) 
    {
        src_pte = pte_at(src);

        if (!pte_isloaded(src_pte))
            goto cont;

        if (pte_huge(src_pte)) {
            copy_leaf(state, dest, src, src_pte);
            goto cont;
        }

        next = copy_root(state, dest, src, src_pte);
        __copy_lft(state, next, (pte_t*)pte_page_va(src_pte), va_from, va_to);
cont:
        i++;
        dest++;
        src++;
        va_from = (va_from + L3T_SIZE) & L3T_MASK;
    }
#else
    __copy_lft(state, dest, src, va_from, va_to);
#endif
}

static inline void
__copy_l2t(struct __vmcpy* state, pte_t* dest, pte_t* src, 
        ptr_t va_from, ptr_t va_to)
{
#if has_ptlevel(L2T)
    pte_t src_pte, *next;
    
    int i = level_index(va_from, L2T);
    dest += i;
    src += i;

    while (!state->err && va_from < va_to && i < L2T_LENGTH) 
    {
        src_pte = pte_at(src);

        if (!pte_isloaded(src_pte))
            goto cont;

        if (pte_huge(src_pte)) {
            copy_leaf(state, dest, src, src_pte);
            goto cont;
        }

        next = copy_root(state, dest, src, src_pte);
        __copy_l3t(state, next, (pte_t*)pte_page_va(src_pte), va_from, va_to);
cont:
        i++;
        dest++;
        src++;
        va_from = (va_from + L2T_SIZE) & L2T_MASK;
    }
#else
    __copy_l3t(state, dest, src, va_from, va_to);
#endif
}


static inline void
__copy_l1t(struct __vmcpy* state, pte_t* dest, pte_t* src, 
        ptr_t va_from, ptr_t va_to)
{
#if has_ptlevel(L1T)
    pte_t src_pte, *next;
    
    int i = level_index(va_from, L1T);
    dest += i;
    src += i;

    while (!state->err && va_from < va_to && i < L1T_LENGTH) 
    {
        src_pte = pte_at(src);

        if (!pte_isloaded(src_pte))
            goto cont;

        if (pte_huge(src_pte)) {
            copy_leaf(state, dest, src, src_pte);
            goto cont;
        }

        next = copy_root(state, dest, src, src_pte);
        __copy_l2t(state, next, (pte_t*)pte_page_va(src_pte), va_from, va_to);
cont:
        i++;
        dest++;
        src++;
        va_from = (va_from + L1T_SIZE) & L1T_MASK;
    }
#else
    __copy_l2t(state, dest, src, va_from, va_to);
#endif
}


static inline void
__copy_va_subspace(struct __vmcpy* state, pte_t* dest, pte_t* src, 
        ptr_t va_from, ptr_t va_to)
{
    pte_t src_pte, *next;
    
    int i = level_index(va_from, L0T);
    dest += i;
    src += i;

    while (!state->err && va_from < va_to && i < L0T_LENGTH) 
    {
        src_pte = pte_at(src);

        if (!pte_isloaded(src_pte))
            goto cont;

        if (pte_huge(src_pte)) {
            copy_leaf(state, dest, src, src_pte);
            goto cont;
        }

        next = copy_root(state, dest, src, src_pte);
        __copy_l1t(state, next, (pte_t*)pte_page_va(src_pte), va_from, va_to);
cont:
        i++;
        dest++;
        src++;
        va_from = (va_from + L0T_SIZE) & L0T_MASK;
    }
}

static inline int
vmscpy(struct proc_mm* dest_mm, struct proc_mm* src_mm)
{
    pte_t *dest, *src;
    struct __vmcpy state = {};
    
    dest = (pte_t*)leaflet_va(alloc_page_table());

    if (!dest)
        return ENOMEM;

    if (!src_mm) {
        src = (pte_t*)phy_to_virt(current_vas());
        goto done;
    }
    
    src = (pte_t*)phy_to_virt(src_mm->vmroot);

    struct mm_region *pos, *n;
    llist_for_each(pos, n, &src_mm->regions, head)
    {
        state.vmr = pos;
        __copy_va_subspace(&state, dest, src, pos->start, pos->end);

        if (state.err)
            return state.err;

        tlb_flush_vmr_all(pos);
    }

done:;
    procvm_link_kernel(dest, src);
    dest_mm->vmroot = virt_to_phy((ptr_t)dest);

    return 0;
}

static enum vastm_action
__free_mappings(struct vastm_state* state, pte_t* ptep, void* data)
{
    pte_t pte;

    pte = pte_at(ptep);
    if (!pte_isloaded(pte))
        return VASTM_CONTINUE;
    
    vastm_visit_next(*state, ptep_next_table(ptep));
    leaflet_return(pte_leaflet(pte));
    set_pte(ptep, null_pte);

    return VASTM_CONTINUE;
}

static inline void 
vmrfree(struct proc_mm* mm, struct mm_region* region)
{
    struct vastm param;

    vastm_param_prepare(&param, NULL);
    vastm_param_cb_set_uniform(&param, __free_mappings);

    vastm_walk(&param, vastm_procvm_root(mm), 
                region->start, region->end, RES_LFT);
}

static void
vmsfree(struct proc_mm* mm)
{
    struct mm_region *pos, *n;

    llist_for_each(pos, n, &mm->regions, head)
    {
        vmrfree(mm, pos);
    }

    procvm_unlink_kernel();
    leaflet_return(leaflet_from_va(phy_to_virt(mm->vmroot)));
}

struct proc_mm*
procvm_create(struct proc_info* proc) 
{
    struct proc_mm* mm = vzalloc(sizeof(struct proc_mm));

    assert(mm);

    mm->heap = 0;
    mm->proc = proc;

    llist_init_head(&mm->regions);
    return mm;
}

void
procvm_dupvms(struct proc_mm* mm) 
{
    struct proc_mm* mm_current;
    assert(__current);

    // FIXME [2026 QUALIFIER] enforce volatile
    mm_current = vmspace(__current);
    mm->heap = mm_current->heap;
    
    vmscpy(mm, mm_current);  
    region_copy_mm(mm_current, mm);
}

void
procvm_initvms_mount(struct proc_mm* mm)
{
    vmscpy(mm, NULL);
}

void
procvm_unmount_release(struct proc_mm* mm) {
    struct mm_region *pos, *n;

    llist_for_each(pos, n, &mm->regions, head)
    {
        mem_flush_pages(pos, pos->start, pos->end, MEM_FLUSH_UNMAP);
        region_release(pos);
    }

    vmsfree(mm);
    vfree(mm);
}

