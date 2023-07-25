#include <lunaix/process.h>

void
proc_init_transfer(struct proc_info* proc,
                   ptr_t stack_top,
                   ptr_t target,
                   int flags)
{
    struct exec_param* execp =
      (struct exec_param*)(stack_top - sizeof(struct exec_param));
    isr_param* isrp = (isr_param*)((ptr_t)execp - sizeof(isr_param));

    *execp = (struct exec_param){
        .cs = KCODE_SEG, .ss = KDATA_SEG, .eip = target, .eflags = cpu_ldstate()
    };

    *isrp = (isr_param){ .registers = { .ds = KDATA_SEG,
                                        .es = KDATA_SEG,
                                        .fs = KDATA_SEG,
                                        .gs = KDATA_SEG },
                         .execp = execp };

    if ((flags & TRANSFER_IE)) {
        execp->eflags |= 0x200;
    }

    proc->intr_ctx = isrp;
}