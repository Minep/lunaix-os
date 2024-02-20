#include <lunaix/mm/procvm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
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

static ptr_t
vmscpy(ptr_t dest_mnt, ptr_t src_mnt, bool only_kernel)
{
    pte_t* ptep_dest    = mkl0tep(mkptep_va(dest_mnt, 0));
    pte_t* ptep         = mkl0tep(mkptep_va(src_mnt, 0));
    pte_t* ptepd_kernel = mkl0tep(mkptep_va(dest_mnt, KERNEL_RESIDENT));
    pte_t* ptep_kernel  = mkl0tep(mkptep_va(src_mnt, KERNEL_RESIDENT));

    // Build the self-reference on dest vms
    pte_t* ptep_sms     = mkptep_va(VMS_SELF, (ptr_t)ptep_dest);
    pte_t* ptep_ssm     = mkptep_va(VMS_SELF, (ptr_t)ptep_sms);
    pte_t  pte_sms      = mkpte_prot(KERNEL_DATA);

    pte_sms = vmm_alloc_page(ptep_ssm, pte_sms);
    set_pte(ptep_sms, pte_sms);    
    
    cpu_flush_page((ptr_t)dest_mnt);

    if (only_kernel) {
        ptep = ptep_kernel;
        ptep_dest += ptep_vfn(ptep_kernel);
    } else {
        ptep++;
        ptep_dest++;
    }

    int level = 0;
    while (ptep < ptep_kernel)
    {
        pte_t pte = *ptep;
        ptr_t pa  = pte_paddr(pte);

        if (pte_isnull(pte)) {
            goto cont;
        } 
        
        if (pt_last_level(level) || pte_huge(pte)) {
            set_pte(ptep_dest, pte);
            
            if (pte_isloaded(pte))
                pmm_ref_page(pa);
        }
        else if (!pt_last_level(level)) {
            vmm_alloc_page(ptep_dest, pte);

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
    while (ptep_vfn(ptep) < MAX_PTEN - 2) {
        pte_t pte = *ptep;
        assert(!pte_isnull(pte));

        set_pte(ptep_dest, pte);
        pmm_ref_page(pte_paddr(pte));
        
        ptep++;
        ptep_dest++;
    }

    return pte_paddr(*(ptep_dest + 1));
}

static void
vmsfree(ptr_t vm_mnt)
{
    pte_t* ptep_head    = mkl0tep(mkptep_va(vm_mnt, 0));
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

        if (pte_isloaded(pte))
            pmm_free_any(pa);

    cont:
        if (ptep_vfn(ptep) == MAX_PTEN - 1) {
            ptep = ptep_step_out(ptep);
            pmm_free_any(pte_paddr(pte_at(ptep)));
            level--;
        }

        ptep++;
    }

    ptr_t self_pa = pte_paddr(ptep_head[MAX_PTEN - 1]);
    pmm_free_any(self_pa);
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
    assert(mm->vm_mnt == VMS_SELF);

    mm->vm_mnt = 0;
}

ptr_t
procvm_enter_remote(struct remote_vmctx* rvmctx, struct proc_mm* mm, 
                    ptr_t remote_base, size_t size)
{
    ptr_t vm_mnt = mm->vm_mnt;
    assert(vm_mnt);
    
    pfn_t size_pn = pfn(size + MEM_PAGE);
    assert(size_pn < REMOTEVM_MAX_PAGES);

    struct mm_region* region = region_get(&mm->regions, remote_base);
    assert(region && region_contains(region, remote_base + size));

    rvmctx->vms_mnt = vm_mnt;
    rvmctx->page_cnt = size_pn;

    remote_base = page_aligned(remote_base);
    rvmctx->remote = remote_base;
    rvmctx->local_mnt = PG_MOUNT_4_END + 1;

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

        ptr_t pa = pmm_alloc_page(0);
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