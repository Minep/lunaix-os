#include <lunaix/trace.h>
#include <asm/aa64_exception.h>
#include <sys-generic/trace_arch.h>

static inline char*
__type_name(reg_t syndrome)
{
    switch (hart_vector_stamp(syndrome))
    {
    case EXCEPTION_SYNC:
        return "sync";
    case EXCEPTION_IRQ:
        return "async (irq)";
    case EXCEPTION_FIQ:
        return "async (fiq)";
    case EXCEPTION_SERR:
        return "async (serr)";
    }

    return "unknwon";
}

void
trace_print_transistion_short(struct hart_state* hstate)
{
    struct exec_param* execp;
    reg_t syndrome;
    
    execp = &hstate->execp;
    syndrome = execp->syndrome;

    trace_log("%s from EL%d: ec=%04x, iss=%08lx, il=%d", 
                __type_name(syndrome), !spsr_from_el0(execp->syndrome),
                esr_ec(syndrome), esr_iss(syndrome), esr_inst32(syndrome));
}

void
trace_print_transition_full(struct hart_state* hstate)
{
    struct exec_param* execp;
    reg_t syndrome;
    
    execp = &hstate->execp;
    syndrome = execp->syndrome;

    trace_log("exception %s from EL%d", 
                __type_name(syndrome), !spsr_from_el0(execp->syndrome));
    trace_log("  ec=0x%08lx, iss=0x%08lx, il=%d",
                esr_ec(syndrome), esr_iss(syndrome), esr_inst32(syndrome));
    trace_log("  esr=0x%016lx,  spsr=0x%016lx",
                syndrome, execp->spsr);
    trace_log("  sp_el0=0x%016lx,  sp_el1=0x%016lx",
                execp->sp_el0, execp->sp_el1);
    trace_log("  pc=0x%016lx", execp->link);
}

void
trace_dump_state(struct hart_state* hstate)
{
    struct regcontext* r;
    
    r = &hstate->registers;

    trace_log("hart state dump (depth=%d)", hstate->depth);

    for (int i = 0; i < 30; i+=3)
    {
        trace_log("  x%02d=0x%016lx   x%02d=0x%016lx   x%02d=0x%016lx",
                    i, r->x[i], 
                    i + 1, r->x[i + 1], 
                    i + 2, r->x[i + 2]);
    }
    
    trace_log("  x30=0x%016lx   x31=0x%016lx (sp)",
                r->x[30],   hart_sp(hstate));
}