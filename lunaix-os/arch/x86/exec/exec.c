#include <lunaix/exec.h>
#include <lunaix/mm/valloc.h>
#include <klibc/string.h>

int
exec_arch_prepare_entry(struct thread* thread, struct exec_host* container)
{
    struct hart_state* hstate;

    hstate = thread->hstate;

#ifdef CONFIG_ARCH_X86_64
    hstate->execp->rip = container->exe.entry;
    hstate->execp->rsp = container->stack_top;

#else
    hstate->execp->eip = container->exe.entry;
    hstate->execp->esp = container->stack_top;

#endif

    return 0;
}