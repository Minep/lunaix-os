#include <lunaix/syscall.h>
#include <lunaix/status.h>

#include <asm/hart.h>
#include "asm/aa64_exception.h"

extern ptr_t syscall_table[__SYSCALL_MAX];

void
aa64_syscall(struct hart_state* hstate)
{
    unsigned int call_id;

    call_id = esr_iss(hstate->execp.syndrome);
    call_id = call_id & 0xffff;

    if (call_id >= __SYSCALL_MAX) {
        return EINVAL;
    }

    if (!syscall_table[call_id]) {
        return EINVAL;
    }

    register reg_t param0 asm("x0") = hstate->registers.x[0];
    register reg_t param1 asm("x1") = hstate->registers.x[1];
    register reg_t param2 asm("x2") = hstate->registers.x[2];
    register reg_t param3 asm("x3") = hstate->registers.x[3];
    register reg_t param4 asm("x4") = hstate->registers.x[4];

    asm volatile (
        "blr %[call_fn]"
        :
        "=r"(param0)
        :
        [call_fn] "r"(syscall_table[call_id]),
        "r"(param0),
        "r"(param1),
        "r"(param2),
        "r"(param3),
        "r"(param4)
    );
    
    hstate->registers.x[0] = param0;
}