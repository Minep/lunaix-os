#ifndef __LUNAIX_LOADER_H
#define __LUNAIX_LOADER_H

#include <lunaix/process.h>
#include <lunaix/types.h>

#define LD_STAT_FKUP 0x1U

#define MAX_VAR_PAGES 8

struct ld_param
{
    struct proc_info* proc;
    ptr_t vms_mnt;
    struct elf32_ehdr ehdr_out;
    int status;
};

int
elf_load(struct ld_param* ldparam, struct v_file* elfile);

void
ld_create_param(struct ld_param* param, struct proc_info* proc, ptr_t vms);

#endif /* __LUNAIX_LOADER_H */
