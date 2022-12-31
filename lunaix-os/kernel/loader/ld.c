#include <lunaix/ld.h>

void
ld_create_param(struct ld_param* param, struct proc_info* proc, ptr_t vms)
{
    *param = (struct ld_param){ .proc = proc, .vms_mnt = vms };
}