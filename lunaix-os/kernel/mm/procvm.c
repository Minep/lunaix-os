#include <lunaix/mm/procvm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/process.h>
#include <lunaix/syslog.h>

#include <asm/mm_defs.h>

#include <klibc/string.h>

#define alloc_pagetable_trace(ptep, pte, ord, level)                        \
    ({                                                                      \
        alloc_kpage_at(ptep, pte, ord);                                     \
    })

#define free_pagetable_trace(ptep, pte, level)                              \
    ({                                                                      \
        struct leaflet* leaflet = pte_leaflet_aligned(pte);                 \
        assert(leaflet_order(leaflet) == 0);                                \
        leaflet_return(leaflet);                                            \
        set_pte(ptep, null_pte);                                            \
    })

struct proc_mm*
procvm_create(struct proc_info* proc) {
    struct proc_mm* mm = vzalloc(sizeof(struct proc_mm));

    assert(mm);

    mm->heap = 0;
    mm->proc = proc;

    llist_init_head(&mm->regions);
    return mm;
}

static inline unsigned int
__ptep_advancement(struct leaflet* leaflet, int level)
{
    size_t shifts = MAX(MAX_LEVEL - level - 1, 1) * LEVEL_SHIFT;
    return (1 << (leaflet_order(leaflet) % shifts)) - 1;
}

static inline int
__descend(ptr_t dest_mnt, ptr_t src_mnt, ptr_t va, bool alloc)
{
    pte_t *dest, *src, pte;

    int i = 0;
    while (!pt_last_level(i))
    {
        dest = mklntep_va(i, dest_mnt, va);
        src  = mklntep_va(i, src_mnt, va);
        pte  = pte_at(src);

        if (!pte_isloaded(pte) || pte_huge(pte)) {
            break;
        }

        if (alloc && pte_isnull(pte_at(dest))) {
            alloc_pagetable_trace(dest, pte, 0, i);
        }

        i++;
    }

    return i;
}

static inline void
copy_leaf(pte_t* dest, pte_t* src, pte_t pte, int level)
{
    struct leaflet* leaflet;

    set_pte(dest, pte);

    if (!pte_isloaded(pte)) {
        return;
    }

    leaflet = pte_leaflet(pte);
    assert(leaflet_refcount(leaflet));
    
    if (leaflet_ppfn(leaflet) == pte_ppfn(pte)) {
        leaflet_borrow(leaflet);
    }
}

static inline void
copy_root(pte_t* dest, pte_t* src, pte_t pte, int level)
{
    alloc_pagetable_trace(dest, pte, 0, level);
}

static void
vmrcpy(ptr_t dest_mnt, ptr_t src_mnt, struct mm_region* region)
{
    pte_t *src, *dest;
    ptr_t loc;
    int level;
    struct leaflet* leaflet;

    loc  = region->start;
    src  = mkptep_va(src_mnt, loc);
    dest = mkptep_va(dest_mnt, loc);

    level = __descend(dest_mnt, src_mnt, loc, true);

    while (loc < region->end)
    {
        pte_t pte = *src;

        if (pte_isnull(pte)) {
            goto cont;
        } 
        
        if (pt_last_level(level) || pte_huge(pte)) {
            copy_leaf(dest, src, pte, level);
            goto cont;
        }
        
        if (!pt_last_level(level)) {
            copy_root(dest, src, pte, level);

            src = ptep_step_into(src);
            dest = ptep_step_into(dest);
            level++;

            continue;
        }
        
    cont:
        loc += lnt_page_size(level);
        while (ptep_vfn(src) == MAX_PTEN - 1) {
            assert(level > 0);
            src = ptep_step_out(src);
            dest = ptep_step_out(dest);
            level--;
        }

        src++;
        dest++;
    }
}

static void
vmrfree(ptr_t vm_mnt, struct mm_region* region)
{
    pte_t *src, *end;
    ptr_t loc;
    int level;
    struct leaflet* leaflet;

    loc  = region->start;
    src  = mkptep_va(vm_mnt, region->start);
    end  = mkptep_va(vm_mnt, region->end);

    level = __descend(vm_mnt, vm_mnt, loc, false);

    while (src < end)
    {
        pte_t pte = *src;
        ptr_t pa  = pte_paddr(pte);

        if (pte_isnull(pte)) {
            goto cont;
        } 

        if (!pt_last_level(level) && !pte_huge(pte)) {
            src = ptep_step_into(src);
            level++;

            continue;
        }

        set_pte(src, null_pte);
        
        if (pte_isloaded(pte)) {
            leaflet = pte_leaflet_aligned(pte);
            leaflet_return(leaflet);

            src += __ptep_advancement(leaflet, level);
        }

    cont:
        while (ptep_vfn(src) == MAX_PTEN - 1) {
            src = ptep_step_out(src);
            free_pagetable_trace(src, pte_at(src), level);
            
            level--;
        }

        src++;
    }
}

static void
vmscpy(struct proc_mm* dest_mm, struct proc_mm* src_mm)
{
    // Build the self-reference on dest vms

    /* 
     *        -- What the heck are ptep_ssm and ptep_sms ? --
     *      
     *      ptep_dest point to the pagetable itself that is mounted
     *          at dest_mnt (or simply mnt): 
     *              mnt -> self -> self -> self -> L0TE@offset
     * 
     *      ptep_sms shallowed the recursion chain:
     *              self -> mnt -> self -> self -> L0TE@self
     * 
     *      ptep_ssm shallowed the recursion chain:
     *              self -> self -> mnt -> self -> L0TE@self
     *      
     *      Now, here is the problem, back to x86_32, the translation is 
     *      a depth-3 recursion:
     *              L0T -> LFT -> Page
     *      
     *      So ptep_ssm will terminate at mnt and give us a leaf
     *      slot for allocate a fresh page table for mnt:
     *              self -> self -> L0TE@mnt
     * 
     *      but in x86_64 translation has extra two more step:
     *              L0T -> L1T -> L2T -> LFT -> Page
     *      
     *      So we must continue push down.... 
     *      ptep_sssms shallowed the recursion chain:
     *              self -> self -> self -> mnt  -> L0TE@self
     * 
     *      ptep_ssssm shallowed the recursion chain:
     *              self -> self -> self -> self -> L0TE@mnt
     * 
     *      Note: PML4: 2 extra steps
     *            PML5: 3 extra steps
    */

    ptr_t  dest_mnt, src_mnt;
    
    dest_mnt = dest_mm->vm_mnt;
    assert(dest_mnt);

    pte_t* ptep_ssm     = mkl0tep_va(VMS_SELF, dest_mnt);
    pte_t* ptep_smx     = mkl1tep_va(VMS_SELF, dest_mnt);
    pte_t  pte_sms      = mkpte_prot(KERNEL_PGTAB);

    pte_sms = alloc_pagetable_trace(ptep_ssm, pte_sms, 0, 0);
    set_pte(&ptep_smx[VMS_SELF_L0TI], pte_sms);
    
    tlb_flush_kernel((ptr_t)dest_mnt);

    if (!src_mm) {
        goto done;
    }

    src_mnt = src_mm->vm_mnt;

    struct mm_region *pos, *n;
    llist_for_each(pos, n, &src_mm->regions, head)
    {
        vmrcpy(dest_mnt, src_mnt, pos);
    }

done:;
    procvm_link_kernel(dest_mnt);
    
    dest_mm->vmroot = pte_paddr(pte_sms);
}

static void
__purge_vms_residual(struct proc_mm* mm, int level, ptr_t va)
{
    pte_t *ptep, pte;
    ptr_t _va;

    if (level >= MAX_LEVEL) {
        return;
    }

    ptep = mklntep_va(level, mm->vm_mnt, va);

    for (unsigned i = 0; i < LEVEL_SIZE; i++, ptep++) 
    {
        pte = pte_at(ptep);
        if (pte_isnull(pte) || !pte_isloaded(pte)) {
            continue;
        }

        if (lntep_implie_vmnts(ptep, lnt_page_size(level))) {
            continue;
        }

        _va = va + (i * lnt_page_size(level));
        __purge_vms_residual(mm, level + 1, _va);
        
        set_pte(ptep, null_pte);
        leaflet_return(pte_leaflet_aligned(pte));
    }
}

static void
vmsfree(struct proc_mm* mm)
{
    struct leaflet* leaflet;
    struct mm_region *pos, *n;
    ptr_t vm_mnt;
    pte_t* ptep_self;
    
    vm_mnt    = mm->vm_mnt;
    ptep_self = mkl0tep_va(vm_mnt, VMS_SELF);

    // first pass: free region mappings
    llist_for_each(pos, n, &mm->regions, head)
    {
        vmrfree(vm_mnt, pos);
    }

    procvm_unlink_kernel(vm_mnt);

    // free up all allocated tables on intermediate levels
    __purge_vms_residual(mm, 0, 0);

    free_pagetable_trace(ptep_self, pte_at(ptep_self), 0);
}

static inline void
__attach_to_current_vms(struct proc_mm* guest_mm)
{
    struct proc_mm* mm_current = vmspace(__current);
    if (mm_current) {
        assert(!mm_current->guest_mm);
        mm_current->guest_mm = guest_mm;
    }
}

static inline void
__detach_from_current_vms(struct proc_mm* guest_mm)
{
    struct proc_mm* mm_current = vmspace(__current);
    if (mm_current) {
        assert(mm_current->guest_mm == guest_mm);
        mm_current->guest_mm = NULL;
    }
}

void
procvm_prune_vmr(ptr_t vm_mnt, struct mm_region* region)
{
    vmrfree(vm_mnt, region);
}

void
procvm_dupvms_mount(struct proc_mm* mm) {
    assert(__current);
    assert(!mm->vm_mnt);

    struct proc_mm* mm_current = vmspace(__current);
    
    __attach_to_current_vms(mm);
   
    mm->heap = mm_current->heap;
    mm->vm_mnt = VMS_MOUNT_1;
    
    vmscpy(mm, mm_current);  
    region_copy_mm(mm_current, mm);
}

void
procvm_mount(struct proc_mm* mm)
{
    // if current mm is already active
    if (active_vms(mm->vm_mnt)) {
        return;
    }
    
    // we are double mounting
    assert(!mm->vm_mnt);
    assert(mm->vmroot);

    vms_mount(VMS_MOUNT_1, mm->vmroot);

    __attach_to_current_vms(mm);

    mm->vm_mnt = VMS_MOUNT_1;
}

void
procvm_unmount(struct proc_mm* mm)
{
    if (active_vms(mm->vm_mnt)) {
        return;
    }
    
    assert(mm->vm_mnt);
    vms_unmount(VMS_MOUNT_1);
    
    struct proc_mm* mm_current = vmspace(__current);
    if (mm_current) {
        mm_current->guest_mm = NULL;
    }

    mm->vm_mnt = 0;
}

void
procvm_initvms_mount(struct proc_mm* mm)
{
    assert(!mm->vm_mnt);

    __attach_to_current_vms(mm);

    mm->vm_mnt = VMS_MOUNT_1;
    vmscpy(mm, NULL);
}

void
procvm_unmount_release(struct proc_mm* mm) {
    ptr_t vm_mnt = mm->vm_mnt;
    struct mm_region *pos, *n;

    llist_for_each(pos, n, &mm->regions, head)
    {
        mem_sync_pages(vm_mnt, pos, pos->start, pos->end - pos->start, 0);
    }

    vmsfree(mm);

    llist_for_each(pos, n, &mm->regions, head)
    {
        region_release(pos);
    }

    vms_unmount(vm_mnt);
    vfree(mm);

    __detach_from_current_vms(mm);
}

void
procvm_mount_self(struct proc_mm* mm) 
{
    assert(!mm->vm_mnt);

    mm->vm_mnt = VMS_SELF;
}

void
procvm_unmount_self(struct proc_mm* mm)
{
    assert(active_vms(mm->vm_mnt));

    mm->vm_mnt = 0;
}

ptr_t
procvm_enter_remote(struct remote_vmctx* rvmctx, struct proc_mm* mm, 
                    ptr_t remote_base, size_t size)
{
    ptr_t vm_mnt = mm->vm_mnt;
    assert(vm_mnt);
    
    pfn_t size_pn = pfn(size + PAGE_SIZE);
    assert(size_pn < REMOTEVM_MAX_PAGES);

    struct mm_region* region = region_get(&mm->regions, remote_base);
    assert(region && region_contains(region, remote_base + size));

    rvmctx->vms_mnt = vm_mnt;
    rvmctx->page_cnt = size_pn;

    remote_base = page_aligned(remote_base);
    rvmctx->remote = remote_base;
    rvmctx->local_mnt = PG_MOUNT_VAR;

    pte_t* rptep = mkptep_va(vm_mnt, remote_base);
    pte_t* lptep = mkptep_va(VMS_SELF, rvmctx->local_mnt);

    pte_t pte, rpte = null_pte;
    rpte = region_tweakpte(region, rpte);

    for (size_t i = 0; i < size_pn; i++)
    {
        pte = vmm_tryptep(rptep, PAGE_SIZE);
        if (pte_isloaded(pte)) {
            set_pte(lptep, pte);
            continue;
        }

        ptr_t pa = ppage_addr(pmm_alloc_normal(0));
        set_pte(lptep, mkpte(pa, KERNEL_DATA));
        set_pte(rptep, pte_setpaddr(rpte, pa));
    }

    return vm_mnt;
    
}

int
procvm_copy_remote_transaction(struct remote_vmctx* rvmctx, 
                   ptr_t remote_dest, void* local_src, size_t sz)
{
    if (remote_dest < rvmctx->remote) {
        return -1;
    }

    ptr_t offset = remote_dest - rvmctx->remote;
    if (pfn(offset + sz) >= rvmctx->page_cnt) {
        return -1;
    }

    memcpy((void*)(rvmctx->local_mnt + offset), local_src, sz);

    return sz;
}

void
procvm_exit_remote(struct remote_vmctx* rvmctx)
{
    pte_t* lptep = mkptep_va(VMS_SELF, rvmctx->local_mnt);
    vmm_unset_ptes(lptep, rvmctx->page_cnt);
}