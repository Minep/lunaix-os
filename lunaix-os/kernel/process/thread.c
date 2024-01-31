#include <lunaix/process.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/syslog.h>

#include <sys/abi.h>
#include <sys/mm/mm_defs.h>

LOG_MODULE("THREAD")

static ptr_t
__alloc_user_thread_stack(struct proc_info* proc, struct mm_region** stack_region, ptr_t vm_mnt)
{
    ptr_t th_stack_top = (proc->thread_count + 1) * USR_STACK_SIZE;
    th_stack_top = PG_ALIGN(USR_STACK_END - th_stack_top);

    struct proc_mm* mm = vmspace(proc);
    struct mmap_param param = { .vms_mnt = vm_mnt,
                                .pvms = mm,
                                .mlen = USR_STACK_SIZE,
                                .proct = PROT_READ | PROT_WRITE,
                                .flags = MAP_ANON | MAP_PRIVATE | MAP_FIXED,
                                .type = REGION_TYPE_STACK };

    int errno = mmap_user((void**)&th_stack_top, stack_region, th_stack_top, NULL, &param);

    if (errno) {
        WARN("failed to create user thread stack: %d", errno);
        return 0;
    }

    return align_stack(th_stack_top + USR_STACK_SIZE - 1);
}

static ptr_t
__alloc_kernel_thread_stack(struct proc_info* proc, ptr_t vm_mnt)
{
    v_mapping mapping;
    ptr_t kstack = PG_ALIGN(KSTACK_AREA_END - KSTACK_SIZE);
    while (kstack >= KSTACK_AREA) {
        if (!vmm_lookupat(vm_mnt, kstack, &mapping)) {
            break;
        }

        kstack -= KSTACK_SIZE;
    }

    if (kstack < KSTACK_AREA) {
        return 0;
    }

    ptr_t pa = pmm_alloc_cpage(PN(KSTACK_SIZE), 0);

    if (!pa) {
        return 0;
    }

    for (size_t i = 0; i < KSTACK_SIZE; i+=MEM_PAGE) {
        vmm_set_mapping(vm_mnt, kstack + i, pa + i, PG_PREM_RW, 0);
    }

    return align_stack(kstack + KSTACK_SIZE - 1);
}

struct thread*
create_thread(struct proc_info* proc, ptr_t vm_mnt, bool with_ustack)
{
    ptr_t ustack = 0;
    struct mm_region* ustack_region = NULL;
    if (with_ustack && 
        !(ustack = __alloc_user_thread_stack(proc, &ustack_region, vm_mnt))) 
    {
        return NULL;
    }

    ptr_t kstack = __alloc_kernel_thread_stack(proc, vm_mnt);
    if (!kstack) {
        mem_unmap_region(vm_mnt, ustack_region);
        return NULL;
    }

    struct thread* th = alloc_thread(proc);
    th->kstack = kstack;
    th->ustack = ustack;

    return th;
}

void
start_thread(struct thread* th, ptr_t vm_mnt, ptr_t entry)
{
    assert(entry);
    
    struct transfer_context transfer;
    if (!kernel_addr(entry)) {
        thread_create_user_transfer(&transfer, th->kstack, th->ustack, entry);
    } else {
        thread_create_kernel_transfer(&transfer, th->kstack, entry);
    }

    inject_transfer_context(vm_mnt, &transfer);
    th->intr_ctx = (isr_param*)transfer.inject;

    commit_thread(th);
}
