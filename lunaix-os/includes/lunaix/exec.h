#ifndef __LUNAIX_EXEC_H
#define __LUNAIX_EXEC_H

#include <lunaix/fs.h>
#include <lunaix/process.h>
#include <lunaix/types.h>

#define MAX_PARAM_LEN    1024
#define MAX_PARAM_SIZE   4096

#define MAX_VAR_PAGES 8
#define DEFAULT_HEAP_PAGES 16

struct load_context
{
    struct exec_host* container;
    ptr_t base;
    ptr_t end;
    ptr_t mem_sz;

    ptr_t entry;
};

struct exec_arrptr
{
    unsigned int len;
    unsigned int size;
    ptr_t raw;
    ptr_t copied;
};


struct exec_host
{
    struct proc_info* proc;
    ptr_t vms_mnt;

    struct load_context exe;

    struct exec_arrptr argv;
    struct exec_arrptr envp;

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

int
exec_arch_prepare_entry(struct thread* thread, struct exec_host* container);

int
exec_load_byname(struct exec_host* container, const char* filename);

int
exec_load(struct exec_host* container, struct v_file* executable);

int
exec_kexecve(const char* filename, const char* argv[], const char* envp[]);

void
exec_init_container(struct exec_host* param,
                    struct thread* thread,
                    ptr_t vms,
                    const char** argv,
                    const char** envp);

#endif /* __LUNAIX_LOADER_H */
