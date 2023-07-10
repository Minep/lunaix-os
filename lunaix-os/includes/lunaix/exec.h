#ifndef __LUNAIX_EXEC_H
#define __LUNAIX_EXEC_H

#include <lunaix/elf.h>
#include <lunaix/fs.h>
#include <lunaix/process.h>
#include <lunaix/types.h>

#define NO_LOADER 0
#define DEFAULT_LOADER "usr/ld"

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

    struct load_context executable;

    ptr_t stack_top;
    ptr_t entry; // mapped to one of {executable|loader}.entry

    int status;
};

struct uexec_param
{
    int argc;
    char** argv;
    int envc;
    char** envp;
} PACKED;

#ifndef __USR_WRAPPER__

int
exec_load_byname(struct exec_container* container,
                 const char* filename,
                 const char** argv,
                 const char** envp);

int
exec_load(struct exec_container* container,
          struct v_file* executable,
          const char** argv,
          const char** envp);

int
exec_kexecve(const char* filename, const char* argv[], const char* envp);

#endif

#endif /* __LUNAIX_LOADER_H */
