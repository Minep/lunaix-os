#include <lunaix/mm/procvm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/process.h>

#include <sys/mm/mm_defs.h>

#include <klibc/string.h>

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

static ptr_t
vmscpy(ptr_t dest_mnt, ptr_t src_mnt, bool only_kernel)
{
    pte_t* ptep_dest    = mkl0tep(mkptep_va(dest_mnt, 0));
    pte_t* ptep         = mkl0tep(mkptep_va(src_mnt, 0));
    pte_t* ptepd_kernel = mkl0tep(mkptep_va(dest_mnt, KERNEL_RESIDENT));
    pte_t* ptep_kernel  = mkl0tep(mkptep_va(src_mnt, KERNEL_RESIDENT));

    // Build the self-reference on dest vms

    /* FIXME this does not scale. It made implicit assumption of
     *      two level translation. 
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
    pte_t* ptep_sms     = mkptep_va(VMS_SELF, (ptr_t)ptep_dest);
    pte_t* ptep_ssm     = mkptep_va(VMS_SELF, (ptr_t)ptep_sms);
    pte_t  pte_sms      = mkpte_prot(KERNEL_DATA);

    pte_sms = alloc_kpage_at(ptep_ssm, pte_sms, 0);
    set_pte(ptep_sms, pte_sms);    
    
    tlb_flush_kernel((ptr_t)dest_mnt);

    if (only_kernel) {
        ptep = ptep_kernel;
        ptep_dest += ptep_vfn(ptep_kernel);
    } else {
        ptep++;
        ptep_dest++;
    }

    int level = 0;
    struct leaflet* leaflet;

    while (ptep < ptep_kernel)
    {
        pte_t pte = *ptep;

        if (pte_isnull(pte)) {
            goto cont;
        } 
        
        if (pt_last_level(level) || pte_huge(pte)) {
            set_pte(ptep_dest, pte);

            if (pte_isloaded(pte)) {
                leaflet = pte_leaflet(pte);
                assert(leaflet_refcount(leaflet));
                
                if (leaflet_ppfn(leaflet) == pte_ppfn(pte)) {
                    leaflet_borrow(leaflet);
                }
            }
        }
        else if (!pt_last_level(level)) {
            alloc_kpage_at(ptep_dest, pte, 0);

            ptep = ptep_step_into(ptep);
            ptep_dest = ptep_step_into(ptep_dest);
            level++;

            continue;
        }
        
    cont:
        if (ptep_vfn(ptep) == MAX_PTEN - 1) {
            assert(level > 0);
            ptep = ptep_step_out(ptep);
            ptep_dest = ptep_step_out(ptep_dest);
            level--;
        }

        ptep++;
        ptep_dest++;
    }

    // Ensure we step back to L0T
    assert(!level);
    assert(ptep_dest == ptepd_kernel);
    
    // Carry over the kernel (exclude last two entry)
    unsigned int i = ptep_vfn(ptep);
    while (i++ < MAX_PTEN) {
        pte_t pte = *ptep;

        if (l0tep_impile_vmnts(ptep)) {
            goto _cont;
        }

        assert(!pte_isnull(pte));

        // Ensure it is a next level pagetable,
        //  we MAY relax this later allow kernel
        //  to have huge leaflet mapped at L0T
        leaflet = pte_leaflet_aligned(pte);
        assert(leaflet_order(leaflet) == 0);

        set_pte(ptep_dest, pte);
        leaflet_borrow(leaflet);
    
    _cont:
        ptep++;
        ptep_dest++;
    }

    return pte_paddr(pte_sms);
}

static void
vmsfree(ptr_t vm_mnt)
{
    struct leaflet* leaflet;
    pte_t* ptep_head    = mkl0tep(mkptep_va(vm_mnt, 0));
    pte_t* ptep_self    = mkl0tep(mkptep_va(vm_mnt, VMS_SELF));
    pte_t* ptep_kernel  = mkl0tep(mkptep_va(vm_mnt, KERNEL_RESIDENT));

    int level = 0;
    pte_t* ptep = ptep_head;
    while (ptep < ptep_kernel)
    {
        pte_t pte = *ptep;
        ptr_t pa  = pte_paddr(pte);

        if (pte_isnull(pte)) {
            goto cont;
        } 

        if (!pt_last_level(level) && !pte_huge(pte)) {
            ptep = ptep_step_into(ptep);
            level++;

            continue;
        }

        if (pte_isloaded(pte)) {
            leaflet = pte_leaflet_aligned(pte);
            leaflet_return(leaflet);

            ptep += __ptep_advancement(leaflet, level);
        }

    cont:
        if (ptep_vfn(ptep) == MAX_PTEN - 1) {
            ptep = ptep_step_out(ptep);
            leaflet = pte_leaflet_aligned(pte_at(ptep));
            
            assert(leaflet_order(leaflet) == 0);
            leaflet_return(leaflet);
            
            level--;
        }

        ptep++;
    }

    leaflet = pte_leaflet_aligned(pte_at(ptep_self));
    leaflet_return(leaflet);
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
procvm_dupvms_mount(struct proc_mm* mm) {
    assert(__current);
    assert(!mm->vm_mnt);

    struct proc_mm* mm_current = vmspace(__current);
    
    __attach_to_current_vms(mm);
   
    mm->heap = mm_current->heap;
    mm->vm_mnt = VMS_MOUNT_1;
    mm->vmroot = vmscpy(VMS_MOUNT_1, VMS_SELF, false);
    
    region_copy_mm(mm_current, mm);
}

void
procvm_mount(struct proc_mm* mm)
{
    assert(!mm->vm_mnt);
    assert(mm->vmroot);

    vms_mount(VMS_MOUNT_1, mm->vmroot);

    __attach_to_current_vms(mm);

    mm->vm_mnt = VMS_MOUNT_1;
}

void
procvm_unmount(struct proc_mm* mm)
{
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
    mm->vmroot = vmscpy(VMS_MOUNT_1, VMS_SELF, true);
}

void
procvm_unmount_release(struct proc_mm* mm) {
    ptr_t vm_mnt = mm->vm_mnt;
    struct mm_region *pos, *n;
    llist_for_each(pos, n, &mm->regions, head)
    {
        mem_sync_pages(vm_mnt, pos, pos->start, pos->end - pos->start, 0);
        region_release(pos);
    }

    vfree(mm);
    vmsfree(vm_mnt);
    vms_unmount(vm_mnt);

    __detach_from_current_vms(mm);
}

void
procvm_mount_self(struct proc_mm* mm) 
{
    assert(!mm->vm_mnt);
    assert(!mm->guest_mm);

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
    unsigned int pattr = region_pteprot(region);

    for (size_t i = 0; i < size_pn; i++)
    {
        pte_t pte = vmm_tryptep(rptep, PAGE_SIZE);
        if (pte_isloaded(pte)) {
            set_pte(lptep, pte);
            continue;
        }

        ptr_t pa = ppage_addr(pmm_alloc_normal(0));
        set_pte(lptep, mkpte(pa, KERNEL_DATA));
        set_pte(rptep, mkpte(pa, pattr));
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