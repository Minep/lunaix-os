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
    struct proc_mm* mm = valloc(sizeof(struct proc_mm));

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
    pte_t* ptep_kernel  = mkl0tep(mkptep_va(src_mnt, KERNEL_RESIDENT));

    // Build the self-reference on dest vms
    pte_t* ptep_sms     = mkptep_va(VMS_SELF, (ptr_t)ptep_dest);
    pte_t* ptep_ssm     = mkptep_va(VMS_SELF, (ptr_t)ptep_sms);
    pte_t  pte_sms      = mkpte_prot(KERNEL_DATA);

    set_pte(ptep_sms, vmm_alloc_page(ptep_ssm, pte_sms));

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
            pmm_ref_page(pa);
        }
        
        if (ptep_vfn(ptep) == MAX_PTEN - 1) {
            ptep = ptep_step_out(ptep);
            ptep_dest = ptep_step_out(ptep_dest);
            level--;
        }
        else if (!pt_last_level(level)) {
            vmm_alloc_page(ptep_dest, pte);

            ptep = ptep_step_into(ptep);
            ptep_dest = ptep_step_into(ptep_dest);
            level++;

            continue;
        }

    cont:
        ptep++;
        ptep_dest++;
    }

    // Ensure we step back to L0T
    assert(!level);
    
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

        if (pt_last_level(level) || pte_huge(pte)) {
            pmm_free_any(pa);
            goto cont;
        } 

        if (ptep_vfn(ptep) == MAX_PTEN - 1) {
            ptep = ptep_step_out(ptep);
            pmm_free_any(pte_paddr(*ptep));
            level--;
        }
        else if (level < MAX_LEVEL - 1) {
            ptep = ptep_step_into(ptep);
            level++;

            continue;
        }

        pmm_free_any(pa);
    cont:
        ptep++;
    }

    ptr_t self_pa = pte_paddr(ptep_head[MAX_LEVEL - 1]);
    pmm_free_any(self_pa);
}

void
procvm_dup_and_mount(ptr_t mnt, struct proc_info* proc) {
   struct proc_mm* mm = vmspace(proc);
   struct proc_mm* mm_current = vmspace(__current);
   
   mm->heap = mm_current->heap;
   mm->vmroot = vmscpy(mnt, VMS_SELF, false);
   
   region_copy_mm(mm_current, mm);
}

void
procvm_init_and_mount(ptr_t mnt, struct proc_info* proc)
{
    struct proc_mm* mm = vmspace(proc);
    mm->vmroot = vmscpy(mnt, VMS_SELF, true);
}

void
procvm_cleanup(ptr_t vm_mnt, struct proc_info* proc) {
    struct mm_region *pos, *n;
    llist_for_each(pos, n, vmregions(proc), head)
    {
        mem_sync_pages(vm_mnt, pos, pos->start, pos->end - pos->start, 0);
        region_release(pos);
    }

    vfree(proc->mm);

    vmsfree(vm_mnt);
}

ptr_t
procvm_enter_remote(struct remote_vmctx* rvmctx, struct proc_mm* mm, 
                    ptr_t vm_mnt, ptr_t remote_base, size_t size)
{
    pfn_t size_pn = pfn(size + MEM_PAGE);
    assert(size_pn < REMOTEVM_MAX_PAGES);

    struct mm_region* region = region_get(&mm->regions, remote_base);
    assert(region && region_contains(region, remote_base + size));

    rvmctx->vms_mnt = vm_mnt;
    rvmctx->page_cnt = size_pn;

    remote_base = va_align(remote_base);
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
procvm_exit_remote_transaction(struct remote_vmctx* rvmctx)
{
    pte_t* lptep = mkptep_va(VMS_SELF, rvmctx->local_mnt);
    vmm_unset_ptes(lptep, rvmctx->page_cnt);
}