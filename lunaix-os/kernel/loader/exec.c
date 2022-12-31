#include <lunaix/elf.h>
#include <lunaix/fs.h>
#include <lunaix/ld.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>

#include <klibc/string.h>

size_t
exec_str_size(const char** str_arr, size_t* length)
{
    const char* chr = *str_arr;
    size_t sz = 0, len = 0;

    while (chr) {
        sz += strlen(chr);
        len++;

        chr = *(str_arr + sz);
    }

    *length = len;
    return sz + 1;
}

int
exec_loadto(struct ld_param* param,
            struct v_file* executable,
            const char** argv,
            const char** envp)
{
    int errno = 0;

    size_t argv_len, envp_len;
    size_t sz_argv = exec_str_size(argv, &argv_len);
    size_t sz_envp = exec_str_size(envp, &envp_len);
    size_t total_sz = ROUNDUP(sz_argv + sz_envp, PG_SIZE);

    if (total_sz / PG_SIZE > MAX_VAR_PAGES) {
        errno = E2BIG;
        goto done;
    }

    if ((errno = elf_load(param, executable))) {
        goto done;
    }

    struct mmap_param map_param = { .regions = &param->proc->mm.regions,
                                    .vms_mnt = param->vms_mnt,
                                    .flags = MAP_ANON | MAP_PRIVATE | MAP_FIXED,
                                    .type = REGION_TYPE_VARS,
                                    .proct = PROT_READ,
                                    .length = MAX_VAR_PAGES * PG_SIZE };

    void* mapped;
    isr_param* intr_ctx = &param->proc->intr_ctx;

    // TODO reinitialize heap

    if (param->vms_mnt == VMS_SELF) {
        // we are loading executable into current addr space
        if ((errno = mem_map(&mapped, NULL, UMMAP_END, NULL, &map_param))) {
            goto done;
        }

        memcpy(mapped, (void*)argv, sz_argv);
        memcpy(mapped + sz_argv, (void*)envp, sz_envp);

        ptr_t* ustack = (void*)USTACK_TOP;
        size_t* argc = &((size_t*)&ustack[-1])[-1];

        ustack[-1] = (ptr_t)mapped;
        *argc = argv_len;

        // TODO handle envp.

        intr_ctx->esp = argc;
    } else {
        // TODO need to find a way to inject argv and envp remotely
        fail("not implemented");
    }

    intr_ctx->eip = param->ehdr_out.e_entry;
    // we will jump to new entry point upon syscall's return
    // so execve will not return from the perspective of it's invoker

done:
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
    struct v_dnode* dnode;
    struct v_file* file;

    if ((errno = vfs_walk_proc(filename, &dnode, NULL, 0))) {
        goto done;
    }

    if ((errno = vfs_open(dnode, &file))) {
        goto done;
    }

    struct ld_param ldparam;
    ld_create_param(&ldparam, __current, VMS_SELF);

    if ((errno = exec_loadto(&ldparam, file, argv, envp))) {
        vfs_pclose(file, __current->pid);

        if ((ldparam.status & LD_STAT_FKUP)) {
            // we fucked up our address space.
            terminate_proc(11451);
            schedule();
            fail("should not reach");
        }
    }

done:
    return errno;
}