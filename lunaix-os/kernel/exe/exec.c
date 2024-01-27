#include <lunaix/exec.h>
#include <lunaix/fs.h>
#include <lunaix/load.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

#include <sys/abi.h>
#include <sys/mm/mempart.h>

#include <klibc/string.h>

void
exec_container(struct exec_container* param,
               struct proc_info* proc,
               ptr_t vms,
               const char** argv,
               const char** envp)
{
    *param = (struct exec_container){ .proc = proc,
                                      .vms_mnt = vms,
                                      .exe = { .container = param },
                                      .argv_pp = { 0, 0 },
                                      .argv = argv,
                                      .envp = envp };
}

size_t
args_ptr_size(const char** paramv)
{
    size_t sz = 0;
    while (*paramv) {
        sz++;
        paramv++;
    }

    return (sz + 1) * sizeof(ptr_t);
}

ptr_t
copy_to_ustack(ptr_t stack_top, ptr_t* paramv)
{
    ptr_t ptr;
    size_t sz = 0;

    while ((ptr = *paramv)) {
        sz = strlen((const char*)ptr) + 1;

        stack_top -= sz;
        memcpy((void*)stack_top, (const void*)ptr, sz);
        *paramv = stack_top;

        paramv++;
    }

    return stack_top;
}

// externed from mm/dmm.c
extern int
create_heap(struct proc_mm* pvms, ptr_t addr);

int
exec_load(struct exec_container* container, struct v_file* executable)
{
    int errno = 0;

    const char **argv = container->argv, **envp = container->envp;
    const char** argv_extra = container->argv_pp;

    argv_extra[0] = executable->dnode->name.value;

    if ((errno = load_executable(&container->exe, executable))) {
        goto done;
    }

    struct proc_mm* pvms = &container->proc->mm;

    if (pvms->heap) {
        mem_unmap_region(container->vms_mnt, pvms->heap);
        pvms->heap = NULL;
    }

    if (!argv_extra[1]) {
        // If loading a statically linked file, then heap remapping we can do,
        // otherwise delayed.
        create_heap(&container->proc->mm, PG_ALIGN(container->exe.end));
    }

    if (container->vms_mnt == VMS_SELF) {
        // we are loading executable into current addr space

        ptr_t ustack = USR_STACK_END;
        size_t argv_len = 0, envp_len = 0;
        ptr_t argv_ptr = 0, envp_ptr = 0;

        if (envp) {
            argv_len = args_ptr_size(envp);
            ustack -= envp_len;
            envp_ptr = ustack;

            memcpy((void*)ustack, (const void*)envp, envp_len);
            ustack = copy_to_ustack(ustack, (ptr_t*)ustack);
        }

        if (argv) {
            argv_len = args_ptr_size(argv);
            ustack -= argv_len;

            memcpy((void*)ustack, (const void**)argv, argv_len);
            for (size_t i = 0; i < 2 && argv_extra[i]; i++) {
                ustack -= sizeof(ptr_t);
                *((ptr_t*)ustack) = (ptr_t)argv_extra[i];
                argv_len += sizeof(ptr_t);
            }

            argv_ptr = ustack;
            ustack = copy_to_ustack(ustack, (ptr_t*)ustack);
        }

        // four args (arg{c|v}, env{c|p}) for main
        struct uexec_param* exec_param = &((struct uexec_param*)ustack)[-1];

        container->stack_top = (ptr_t)exec_param;

        *exec_param =
          (struct uexec_param){ .argc = (argv_len - 1) / sizeof(ptr_t),
                                .argv = (char**)argv_ptr,
                                .envc = (envp_len - 1) / sizeof(ptr_t),
                                .envp = (char**)envp_ptr };
    } else {
        /*
            TODO need to find a way to inject argv and envp remotely
                 this is for the support of kernel level implementation of
                 posix_spawn

            IDEA
                1. Allocate a orphaned physical page (i.e., do not belong to any
                VMA)
                2. Mounted to a temporary mount point in current VMA, (i.e.,
                PG_MOUNT_*)
                3. Do setup there.
                4. Unmount then mounted to the foreign VMA as the first stack
                page.
        */
        fail("not implemented");
    }

done:
    return errno;
}

int
exec_load_byname(struct exec_container* container, const char* filename)
{
    int errno = 0;
    struct v_dnode* dnode;
    struct v_file* file;

    if ((errno = vfs_walk_proc(filename, &dnode, NULL, 0))) {
        goto done;
    }

    if ((errno = vfs_open(dnode, &file))) {
        goto done;
    }

    errno = exec_load(container, file);

done:
    return errno;
}

int
exec_kexecve(const char* filename, const char* argv[], const char* envp[])
{
    int errno = 0;
    struct exec_container container;

    exec_container(
      &container, (struct proc_info*)__current, VMS_SELF, argv, envp);

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

__DEFINE_LXSYSCALL3(int,
                    execve,
                    const char*,
                    filename,
                    const char*,
                    argv[],
                    const char*,
                    envp[])
{
    int errno = 0;
    struct exec_container container;

    exec_container(
      &container, (struct proc_info*)__current, VMS_SELF, argv, envp);

    if ((errno = exec_load_byname(&container, filename))) {
        goto done;
    }

    // we will jump to new entry point (_u_start) upon syscall's
    // return so execve 'will not return' from the perspective of it's invoker
    eret_target(__current) = container.exe.entry;
    eret_stack(__current) = container.stack_top;

    // these become meaningless once execved!
    __current->ustack_top = 0;
    signal_reset_context(__current->sigctx);

done:
    // set return value
    store_retval(DO_STATUS(errno));

    // Always yield the process that want execve!
    schedule();

    // this will never get executed!
    return -1;
}