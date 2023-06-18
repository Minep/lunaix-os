#include <lunaix/elf.h>
#include <lunaix/fs.h>
#include <lunaix/ld.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>
#include <lunaix/status.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

#include <klibc/string.h>

size_t
exec_str_size(const char** str_arr, size_t* length)
{
    if (!str_arr) {
        *length = 0;
        return 0;
    }

    const char* chr = *str_arr;
    size_t sz = 0, len = 0;

    while (chr) {
        sz += strlen(chr);
        len++;

        chr = *(str_arr++);
    }

    *length = len;
    return sz + sizeof(char*);
}

void
__heap_copied(struct mm_region* region)
{
    mm_index((void**)&region->proc_vms->heap, region);
}

int
__exec_remap_heap(struct ld_param* param, struct proc_mm* pvms)
{
    if (pvms->heap) {
        mem_unmap_region(param->vms_mnt, pvms->heap);
    }

    struct mmap_param map_param = { .pvms = pvms,
                                    .vms_mnt = param->vms_mnt,
                                    .flags = MAP_ANON | MAP_PRIVATE,
                                    .type = REGION_TYPE_HEAP,
                                    .proct = PROT_READ | PROT_WRITE,
                                    .mlen = PG_SIZE };
    int status = 0;
    struct mm_region* heap;
    if ((status = mem_map(NULL, &heap, param->info.end, NULL, &map_param))) {
        param->status |= LD_STAT_FKUP;
        return status;
    }

    heap->region_copied = __heap_copied;
    mm_index((void**)&pvms->heap, heap);

    return status;
}

int
exec_load(struct ld_param* param,
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

    struct proc_mm* pvms = &param->proc->mm;
    struct mmap_param map_vars = { .pvms = pvms,
                                   .vms_mnt = param->vms_mnt,
                                   .flags = MAP_ANON | MAP_PRIVATE | MAP_FIXED,
                                   .type = REGION_TYPE_VARS,
                                   .proct = PROT_READ,
                                   .mlen = MAX_VAR_PAGES * PG_SIZE };

    void* mapped;

    if ((errno = __exec_remap_heap(param, pvms))) {
        goto done;
    }

    if ((errno = mem_map(&mapped, NULL, UMMAP_END, NULL, &map_vars))) {
        goto done;
    }

    if (param->vms_mnt == VMS_SELF) {
        // we are loading executable into current addr space

        // make some handy infos available to user space
        ptr_t arg_start = mapped + sizeof(struct usr_exec_param);
        if (argv)
            memcpy(arg_start, (void*)argv, sz_argv);
        if (envp)
            memcpy(arg_start + sz_argv, (void*)envp, sz_envp);

        ptr_t* ustack = (ptr_t*)USTACK_TOP;
        struct usr_exec_param* exec_param = (struct usr_exec_param*)mapped;

        ustack[-1] = (ptr_t)mapped;
        param->info.stack_top = &ustack[-1];

        *exec_param = (struct usr_exec_param){ .argc = argv_len,
                                               .argv = arg_start,
                                               .envc = envp_len,
                                               .envp = arg_start + sz_argv,
                                               .info = param->info };
    } else {
        // TODO need to find a way to inject argv and envp remotely
        //      this is for the support of kernel level implementation of
        //      posix_spawn
        fail("not implemented");
    }

    param->info.entry = param->info.ehdr_out.e_entry;
done:
    return errno;
}

int
exec_load_byname(struct ld_param* param,
                 const char* filename,
                 const char** argv,
                 const char** envp)
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

    if ((errno = exec_load(param, file, argv, envp))) {
        vfs_pclose(file, __current->pid);
    }

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
    struct ld_param ldparam;
    ld_create_param(&ldparam, __current, VMS_SELF);

    if ((errno = exec_load_byname(&ldparam, filename, argv, envp))) {
        if ((ldparam.status & LD_STAT_FKUP)) {
            // we fucked up our address space.
            terminate_proc(11451);
            schedule();
            fail("should not reach");
        }
        goto done;
    }

    volatile struct exec_param* execp = __current->intr_ctx.execp;
    execp->esp = ldparam.info.stack_top;
    execp->eip = ldparam.info.entry;

    // we will jump to new entry point (_u_start) upon syscall's
    // return so execve 'will not return' from the perspective of it's invoker

done:
    return DO_STATUS(errno);
}