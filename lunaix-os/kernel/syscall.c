#include <lunaix/process.h>
#include <lunaix/kpreempt.h>

typedef reg_t (*syscall_fn)(reg_t p1, reg_t p2, reg_t p3, reg_t p4, reg_t p5);

reg_t
dispatch_syscall(void* syscall_fnptr, 
                 reg_t p1, reg_t p2, reg_t p3, reg_t p4, reg_t p5)
{
    reg_t ret_val;
    
    thread_stats_update_entering(true);
    
    set_preemption();
    ret_val = ((syscall_fn)syscall_fnptr)(p1, p2, p3, p4, p5);
    no_preemption();

    return ret_val;
}