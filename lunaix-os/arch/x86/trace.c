#include <lunaix/trace.h>

void
trace_print_transistion_short(struct hart_state* hstate)
{
    trace_log("  trigger: iv=%d, ecause=%p, frame=%p",
                hart_vector_stamp(hstate),
                hart_ecause(hstate),
                hart_stack_frame(hstate));
}

#ifdef CONFIG_ARCH_X86_64

void
trace_print_transition_full(struct hart_state* hstate)
{
    trace_log("hart state transition");
    trace_log("  vector=%d, ecause=0x%x", 
                hart_vector_stamp(hstate),
                hart_ecause(hstate));

    trace_log("  rflags=0x%016lx", hstate->execp->rflags);
    trace_log("  sp=0x%016lx, seg_sel=0x%04x", 
                hstate->execp->rsp, 
                hstate->execp->ss);
    trace_log("  ip=0x%016lx, seg_sel=0x%04x",
                hstate->execp->rip,
                hstate->execp->cs);
}

void
trace_dump_state(struct hart_state* hstate)
{
    struct regcontext* rh = &hstate->registers;
    struct exec_param* ep = hstate->execp;
    trace_log("hart state dump (depth=%d)", hstate->depth);
    trace_log("  rax=0x%016lx, rbx=0x%016lx",
                rh->rax, rh->rbx);
    trace_log("  rcx=0x%016lx, rdx=0x%016lx",
                rh->rcx, rh->rdx);
    trace_log("  rdi=0x%016lx, rsi=0x%016lx",
                rh->rdi, rh->rsi);

    trace_log("  r08=0x%016lx, r09=0x%016lx",
                rh->r8, rh->r9);
    trace_log("  r10=0x%016lx, r11=0x%016lx",
                rh->r10, rh->r11);
    trace_log("  r12=0x%016lx, r13=0x%016lx",
                rh->r12, rh->r13);
    trace_log("  r14=0x%016lx, r15=0x%016lx",
                rh->r14, rh->r15);

    trace_log("   cs=0x%04x, rip=0x%016lx",
                ep->cs, ep->rip);
    trace_log("   ss=0x%04x, rsp=0x%016lx",
                ep->ss, ep->rsp);
    trace_log("  rflags=0x%016lx",
                ep->rflags);
}

#else

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
                hstate->execp->ss);
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
                ep->ss, ep->esp);
    trace_log("  eflags=0x%08x",
                ep->eflags);
}

#endif