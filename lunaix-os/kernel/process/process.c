#include <klibc/string.h>
#include <lunaix/clock.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>

#include <sys/abi.h>
#include <sys/mm/mempart.h>

LOG_MODULE("PROC")

void
__del_pagetable(pid_t pid, ptr_t mount_point)
{
    x86_page_table* pptd = (x86_page_table*)(mount_point | (0x3FF << 12));

    // only remove user address space
    for (size_t i = 0; i < L1_INDEX(KERNEL_EXEC); i++) {
        x86_pte_t ptde = pptd->entry[i];
        if (!ptde || !(ptde & PG_PRESENT)) {
            continue;
        }

        x86_page_table* ppt = (x86_page_table*)(mount_point | (i << 12));

        for (size_t j = 0; j < PG_MAX_ENTRIES; j++) {
            x86_pte_t pte = ppt->entry[j];
            // free the 4KB data page
            if ((pte & PG_PRESENT)) {
                pmm_free_page(PG_ENTRY_ADDR(pte));
            }
        }
        // free the L2 page table
        pmm_free_page(PG_ENTRY_ADDR(ptde));
    }
    // free the L1 directory
    pmm_free_page(PG_ENTRY_ADDR(pptd->entry[PG_MAX_ENTRIES - 1]));
}

__DEFINE_LXSYSCALL(pid_t, getpid)
{
    return __current->pid;
}

__DEFINE_LXSYSCALL(pid_t, getppid)
{
    return __current->parent->pid;
}

__DEFINE_LXSYSCALL(pid_t, getpgid)
{
    return __current->pgid;
}

__DEFINE_LXSYSCALL2(int, setpgid, pid_t, pid, pid_t, pgid)
{
    struct proc_info* proc = pid ? get_process(pid) : __current;

    if (!proc) {
        __current->k_status = EINVAL;
        return -1;
    }

    pgid = pgid ? pgid : proc->pid;

    struct proc_info* gruppenfuhrer = get_process(pgid);

    if (!gruppenfuhrer || proc->pgid == gruppenfuhrer->pid) {
        __current->k_status = EINVAL;
        return -1;
    }

    llist_delete(&proc->grp_member);
    llist_append(&gruppenfuhrer->grp_member, &proc->grp_member);

    proc->pgid = pgid;
    return 0;
}

static void
__stack_copied(struct mm_region* region)
{
    mm_index((void**)&region->proc_vms->stack, region);
}

void
init_proc_user_space(struct proc_info* pcb)
{
    vmm_mount_pd(VMS_MOUNT_1, pcb->page_table);

    /*---  分配用户栈  ---*/

    struct mm_region* mapped;
    struct proc_mm* mm = vmspace(pcb);
    struct mmap_param param = { .vms_mnt = VMS_MOUNT_1,
                                .pvms = mm,
                                .mlen = USR_STACK_SIZE,
                                .proct = PROT_READ | PROT_WRITE,
                                .flags = MAP_ANON | MAP_PRIVATE | MAP_FIXED,
                                .type = REGION_TYPE_STACK };

    int status = 0;
    if ((status = mem_map(NULL, &mapped, USR_STACK, NULL, &param))) {
        kprintf(KFATAL "fail to alloc user stack: %d", status);
    }

    mapped->region_copied = __stack_copied;
    mm_index((void**)&mm->stack, mapped);

    // TODO other uspace initialization stuff

    vmm_unmount_pd(VMS_MOUNT_1);
}
