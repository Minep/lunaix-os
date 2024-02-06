#ifndef __LUNAIX_EXEC_H
#define __LUNAIX_EXEC_H

#include <lunaix/fs.h>
#include <lunaix/process.h>
#include <lunaix/types.h>

#define MAX_VAR_PAGES 8
#define DEFAULT_HEAP_PAGES 16

struct exec_context;

struct load_context
{
    struct exec_container* container;
    ptr_t base;
    ptr_t end;
    ptr_t mem_sz;

    ptr_t entry;
};

struct exec_container
{
    struct proc_info* proc;
    ptr_t vms_mnt;

    struct load_context exe;

    // argv prependums
    const char* argv_pp[2];
    const char** argv;
    const char** envp;

    ptr_t stack_top;

    int status;
};

struct uexec_param
{
    int argc;
    char** argv;
    int envc;
    char** envp;
} compact;

#ifndef __USR_WRAPPER__

int
exec_load_byname(struct exec_container* container, const char* filename);

int
exec_load(struct exec_container* container, struct v_file* executable);

int
exec_kexecve(const char* filename, const char* argv[], const char* envp[]);

void
exec_init_container(struct exec_container* param,
                    struct thread* thread,
                    ptr_t vms,
                    const char** argv,
                    const char** envp);

#endif

#endif /* __LUNAIX_LOADER_H */
