#include <lunaix/exec.h>
#include <lunaix/fs.h>
#include <lunaix/load.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

#include <asm/abi.h>
#include <asm/mm_defs.h>

#include <klibc/string.h>

#define _push(ptr, type, val)               \
    do {                                    \
        ptr = __ptr(&((type*)ptr)[-1]);   \
        *((type*)ptr) = (val);            \
    } while(0)

static int
__place_arrayptrs(struct exec_host* container, struct exec_arrptr* param)
{
    int len;
    ptr_t usp, usp_top;
    ptr_t* ptrs;
    unsigned int* reloc_off;
    
    usp_top = container->stack_top;
    usp  = usp_top;
    ptrs = (ptr_t*)param->raw;

    if (!param->len) {
        _push(usp, unsigned int, 0);

        goto done;
    }

    len = param->len;
    reloc_off = valloc(len * sizeof(unsigned int));

    char* el;
    size_t el_sz, sz_acc = 0;
    for (int i = 0; i < len; i++)
    {
        el= (char*)ptrs[i];
        el_sz = strnlen(el, MAX_PARAM_SIZE) + 1;

        usp -= el_sz;
        sz_acc += el_sz;
        strncpy((char*)usp, el, el_sz);
        
        reloc_off[i] = sz_acc;
    }

    param->size = sz_acc;
    
    ptr_t* toc = (ptr_t*)(usp) - 1;
    toc[0] = 0;

    toc = &toc[-1];
    for (int i = 0, j = len - 1; i < len; i++, j--)
    {
        toc[-i] = usp_top - (ptr_t)reloc_off[j];
    }

    toc[-len] = (ptr_t)len;

    usp = __ptr(&toc[-len]);
    param->copied = __ptr(&toc[-len + 1]);

    vfree(reloc_off);

done:
    container->stack_top = usp;
    return 0;
}

void
exec_init_container(struct exec_host* param,
               struct thread* thread,
               ptr_t vms,
               const char** argv,
               const char** envp)
{
    assert(thread->ustack);
    ptr_t ustack_top = align_stack(thread->ustack->end - 1);
    *param = (struct exec_host) 
    { 
        .proc = thread->process,
        .vms_mnt = vms,
        .exe = { 
            .container = param 
        },
        .argv = { 
            .raw = __ptr(argv) 
        },
        .envp = {
            .raw = __ptr(envp)
        },
        .stack_top = ustack_top 
    };
}

int
count_length(struct exec_arrptr* param)
{
    int i = 0;
    ptr_t* arr = (ptr_t*)param->raw;

    if (!arr) {
        param->len = 0;
        return 0;
    }

    for (; i < MAX_PARAM_LEN && arr[i]; i++);
    
    param->len = i;
    return i > 0 && arr[i] ? E2BIG : 0;
}

static void
save_process_cmd(struct proc_info* proc, struct exec_arrptr* argv)
{
    ptr_t ptr, *argv_ptrs;
    char* cmd_;

    if (proc->cmd) {
        vfree(proc->cmd);
    }

    argv_ptrs = (ptr_t*)argv->copied;
    cmd_  = (char*)valloc(argv->size);

    proc->cmd = cmd_;
    while ((ptr = *argv_ptrs)) {
        cmd_ = strcpy(cmd_, (const char*)ptr);
        cmd_[-1] = ' ';
        argv_ptrs++;
    }
    cmd_[-1] = '\0';
    
    proc->cmd_len = argv->size;
}


static int
__place_params(struct exec_host* container)
{
    int errno = 0;

    errno = __place_arrayptrs(container, &container->envp);
    if (errno) {
        goto done;
    }

    errno = __place_arrayptrs(container, &container->argv);

done:
    return errno;
}

int
exec_load(struct exec_host* container, struct v_file* executable)
{
    int errno = 0;

    struct exec_arrptr* argv = &container->argv;
    struct exec_arrptr* envp = &container->envp;

    struct proc_info* proc = container->proc;
    struct proc_mm* pvms = vmspace(proc);

    if (pvms->heap) {
        mem_unmap_region(container->vms_mnt, pvms->heap);
        pvms->heap = NULL;
    }

    if (!active_vms(container->vms_mnt)) {
        /*
            TODO Setup remote mapping of user stack for later use
        */
        fail("not implemented");

    }

    if ((errno = count_length(argv))) {
        goto done;
    }

    if ((errno = count_length(envp))) {
        goto done;
    }

    errno = __place_params(container);
    if (errno) {
        goto done;
    }

    save_process_cmd(proc, argv);
    container->inode = executable->inode;
    
    errno = load_executable(&container->exe, executable);
    if (errno) {
        goto done;
    }

done:
    return errno;
}

int
exec_load_byname(struct exec_host* container, const char* filename)
{
    int errno = 0;
    struct v_dnode* dnode;
    struct v_file* file;

    if ((errno = vfs_walk_proc(filename, &dnode, NULL, 0))) {
        goto done;
    }

    if (!check_allow_execute(dnode->inode)) {
        errno = EPERM;
        goto done;
    }

    if (!check_itype_any(dnode->inode, F_FILE)) {
        errno = EISDIR;
        goto done;
    }
    
    if ((errno = vfs_open(dnode, &file))) {
        goto done;
    }

    errno = exec_load(container, file);

    // It shouldn't matter which pid we passed. As the only reader is 
    //  in current context and we must finish read at this point,
    //  therefore the dead-lock condition will not exist and the pid
    //  for arbitration has no use.
    vfs_pclose(file, container->proc->pid);

done:
    return errno;
}

int
exec_kexecve(const char* filename, const char* argv[], const char* envp[])
{
    int errno = 0;
    struct exec_host container;

    assert(argv && envp);

    exec_init_container(&container, current_thread, VMS_SELF, argv, envp);

    errno = exec_load_byname(&container, filename);

    if (errno) {
        return errno;
    }

    ptr_t entry = container.exe.entry;

    assert(entry);
    j_usr(container.stack_top, entry);

    // should not reach

    return errno;
}

__DEFINE_LXSYSCALL3(int, execve, const char*, filename,
                    const char*, argv[], const char*, envp[])
{
    int errno = 0;
    int acl;
    struct exec_host container;

    if (!argv || !envp) {
        errno = EINVAL;
        goto done;
    }

    exec_init_container(
      &container, current_thread, VMS_SELF, argv, envp);

    if ((errno = exec_load_byname(&container, filename))) {
        goto done;
    }

    // we will jump to new entry point (_u_start) upon syscall's
    // return so execve 'will not return' from the perspective of it's invoker
    exec_arch_prepare_entry(current_thread, &container);

    // these become meaningless once execved!
    current_thread->ustack_top = 0;
    signal_reset_context(&current_thread->sigctx);
    signal_reset_registry(__current->sigreg);

    acl = container.inode->acl;
    if (fsacl_test(acl, suid)) {
        current_set_euid(container.inode->uid);
    }

    if (fsacl_test(acl, sgid)) {
        current_set_egid(container.inode->gid);
    }

done:
    // set return value
    store_retval(DO_STATUS(errno));

    // Always yield the process that want execve!
    schedule();

    unreachable;
}