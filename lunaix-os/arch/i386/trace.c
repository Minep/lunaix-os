#include <lunaix/trace.h>

void
trace_print_transistion_short(struct hart_state* hstate)
{
    trace_log("  trigger: iv=%d, ecause=%p",
                hart_vector_stamp(hstate),
                hart_ecause(hstate));
}

void
trace_print_transition_full(struct hart_state* hstate)
{
    trace_log("hart state transition");
    trace_log("  vector=%d, ecause=0x%x", 
                hart_vector_stamp(hstate),
                hart_ecause(hstate));
    trace_log("  eflags=0x%x", hstate->execp->eflags);
    trace_log("  sp=%p, [seg_sel=0x%04x]", 
                hstate->execp->esp, 
                hstate->execp->esp);
    trace_log("  ip=%p, seg_sel=0x%04x",
                hstate->execp->eip,
                hstate->execp->cs);
}

void
trace_dump_state(struct hart_state* hstate)
{
    struct regcontext* rh = &hstate->registers;
    struct exec_param* ep = hstate->execp;
    trace_log("hart state dump (depth=%d)", hstate->depth);
    trace_log("  eax=0x%08x, ebx=0x%08x, ecx=0x%08x",
                rh->eax, rh->ebx, rh->ecx);
    trace_log("  edx=0x%08x, ebp=0x%08x",
                rh->edx, rh->ebp);
    trace_log("   ds=0x%04x, edi=0x%08x",
                rh->ds, rh->edi);
    trace_log("   es=0x%04x, esi=0x%08x",
                rh->es, rh->esi);
    trace_log("   fs=0x%04x, gs=0x%x",
                rh->fs, rh->gs);
    trace_log("   cs=0x%04x, ip=0x%08x",
                ep->cs, ep->eip);
    trace_log("  [ss=0x%04x],sp=0x%08x",
                ep->ss, ep->eip);
    trace_log("  eflags=0x%08x",
                ep->eflags);
}