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
                                    .flags = MAP_ANON | MAP_PRIVATE | MAP_FIXED,
                                    .type = REGION_TYPE_HEAP,
                                    .proct = PROT_READ | PROT_WRITE,
                                    .mlen = DEFAULT_HEAP_PAGES * PG_SIZE };
    int status = 0;
    struct mm_region* heap;
    if ((status = mem_map(NULL, &heap, param->info.end, NULL, &map_param))) {
        param->status |= LD_STAT_FKUP;
        return status;
    }

    heap->region_copied = __heap_copied;
    mm_index((void**)&pvms->heap, heap);
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

    struct proc_mm* pvms = &param->proc->mm;
    struct mmap_param map_vars = { .pvms = pvms,
                                   .vms_mnt = param->vms_mnt,
                                   .flags = MAP_ANON | MAP_PRIVATE | MAP_FIXED,
                                   .type = REGION_TYPE_VARS,
                                   .proct = PROT_READ,
                                   .mlen = MAX_VAR_PAGES * PG_SIZE };

    void* mapped;
    isr_param* intr_ctx = &param->proc->intr_ctx;

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
        memcpy(arg_start, (void*)argv, sz_argv);
        memcpy(arg_start + sz_argv, (void*)envp, sz_envp);

        struct usr_exec_param* param = mapped;
        *param = (struct usr_exec_param){ .argc = argv_len,
                                          .argv = arg_start,
                                          .envc = envp_len,
                                          .envp = arg_start + sz_argv,
                                          .info = param->info };
        ptr_t* ustack = (ptr_t*)USTACK_TOP;
        ustack[-1] = (ptr_t)mapped;
        intr_ctx->esp = &ustack[-1];
    } else {
        // TODO need to find a way to inject argv and envp remotely
        fail("not implemented");
    }

    intr_ctx->eip = param->info.ehdr_out.e_entry;
    // we will jump to new entry point (_u_start) upon syscall's
    // return so execve 'will not return' from the perspective of it's invoker

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