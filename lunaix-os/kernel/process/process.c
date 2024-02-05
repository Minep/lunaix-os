#include <klibc/string.h>
#include <lunaix/clock.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/mm/region.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>
#include <lunaix/exec.h>
#include <lunaix/fs.h>

#include <sys/abi.h>
#include <sys/mm/mm_defs.h>

LOG_MODULE("PROC")

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
        syscall_result(EINVAL);
        return -1;
    }

    pgid = pgid ? pgid : proc->pid;

    struct proc_info* gruppenfuhrer = get_process(pgid);

    if (!gruppenfuhrer || proc->pgid == gruppenfuhrer->pid) {
        syscall_result(EINVAL);
        return -1;
    }

    llist_delete(&proc->grp_member);
    llist_append(&gruppenfuhrer->grp_member, &proc->grp_member);

    proc->pgid = pgid;
    return 0;
}

int
spawn_process(struct thread** created, ptr_t entry, bool with_ustack) 
{
    struct proc_info* kproc = alloc_process();

    procvm_init_clean(kproc);

    vmm_mount_pd(VMS_MOUNT_1, vmroot(kproc));
    
    struct thread* kthread = create_thread(kproc, VMS_MOUNT_1, with_ustack);

    if (!kthread) {
        vmm_unmount_pd(VMS_MOUNT_1);
        delete_process(kproc);
        return -1;
    }

    commit_process(kproc);
    start_thread(kthread, VMS_MOUNT_1, entry);

    vmm_unmount_pd(VMS_MOUNT_1);

    if (created) {
        *created = kthread;
    }

    return 0;
}

int
spawn_process_usr(struct thread** created, char* path, 
                    const char** argv, const char** envp)
{
    // FIXME remote injection of user stack not yet implemented

    struct proc_info* proc = alloc_process();
    
    assert(!kernel_process(proc));

    procvm_init_clean(proc);

    vmm_mount_pd(VMS_MOUNT_1, vmroot(proc));

    int errno = 0;
    struct thread* main_thread;
    if (!(main_thread = create_thread(proc, VMS_MOUNT_1, true))) {
        errno = ENOMEM;
        goto fail;
    }

    struct exec_container container;
    exec_init_container(&container, main_thread, VMS_MOUNT_1, argv, envp);
    if ((errno = exec_load_byname(&container, path))) {
        goto fail;
    }

    commit_process(proc);
    start_thread(main_thread, VMS_MOUNT_1, container.exe.entry);

    if (created) {
        *created = main_thread;
    }

    vmm_unmount_pd(VMS_MOUNT_1);
    return 0;

fail:
    vmm_unmount_pd(VMS_MOUNT_1);
    delete_process(proc);
    return errno;
}


ptr_t proc_vmroot() {
    return __current->mm->vmroot;
}