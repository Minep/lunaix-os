#ifndef __LUNAIX_LOADER_H
#define __LUNAIX_LOADER_H

#include <lunaix/elf.h>
#include <lunaix/process.h>
#include <lunaix/types.h>

#define LD_STAT_FKUP 0x1U

#define MAX_VAR_PAGES 8
#define DEFAULT_HEAP_PAGES 16

struct ld_info
{
    struct elf32_ehdr ehdr_out;
    ptr_t base;
    ptr_t end;
    ptr_t mem_sz;
};

struct ld_param
{
    struct proc_info* proc;
    ptr_t vms_mnt;

    struct ld_info info;
    int status;
};

struct usr_exec_param
{
    int argc;
    char** argv;
    int envc;
    char** envp;
    struct ld_info info;
} PACKED;

#ifndef __USR_WRAPPER__
int
elf_load(struct ld_param* ldparam, struct v_file* elfile);

void
ld_create_param(struct ld_param* param, struct proc_info* proc, ptr_t vms);
#endif

#endif /* __LUNAIX_LOADER_H */
