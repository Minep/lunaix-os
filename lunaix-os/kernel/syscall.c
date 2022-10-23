#include <arch/x86/interrupts.h>
#include <lunaix/isrm.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>

LOG_MODULE("SYSCALL")

extern void
syscall_hndlr(const isr_param* param);

void
syscall_install()
{
    isrm_bindiv(LUNAIX_SYS_CALL, syscall_hndlr);
}