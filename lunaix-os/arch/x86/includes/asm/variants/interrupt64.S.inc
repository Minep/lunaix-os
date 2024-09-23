
#define regsize     8
#define regsize32   4

/* stack layout: saved interrupt context */
    .struct 0
idepth:
    .struct idepth + regsize
irax:
    .struct irax + regsize
irbx:
    .struct irbx + regsize
ircx:
    .struct ircx + regsize
irdx:
    .struct irdx + regsize
irdi:
    .struct irdi + regsize
irbp:
    .struct irbp + regsize
irsi:
    .struct irsi + regsize
ir8:
    .struct ir8 + regsize
ir9:
    .struct ir9 + regsize
ir10:
    .struct ir10 + regsize
ir11:
    .struct ir11 + regsize
ir12:
    .struct ir12 + regsize
ir13:
    .struct ir13 + regsize
ir14:
    .struct ir14 + regsize
ir15:
    .struct ir15 + regsize
iexecp:
irsp:
    .struct irsp + regsize
isave_parent:
    .struct isave_parent + regsize
ivec:
    .struct ivec + regsize
iecode:
    .struct iecode + regsize
irip:
    .struct irip + regsize
ics:
    .struct ics + regsize
irflags:
    .struct irflags + regsize
iursp:
    .struct iursp + regsize
iuss:


/* stack layout: execution (flow-control) state context */
    .struct 0
exsave_prev:
    .struct exsave_prev + regsize
exvec:
    .struct exvec + regsize
execode:
    .struct execode + regsize
exrip:
    .struct exrip + regsize
excs:
    .struct excs + regsize
exrflags:
    .struct exrflags + regsize
exursp:
    .struct exursp + regsize
exuss:

/* struct layout: critical section of struct proc_info */
    .struct 0
thread_hstate:
    .struct thread_hstate + regsize
thread_ustack_top:

/* struct layout: proc_sig */
    .struct 0
psig_signum:
    .struct psig_signum + regsize32
psig_sigact:
    .struct psig_sigact + regsize
psig_sighand:
    .struct psig_sighand + regsize
psig_saved_hstate:

/* struct layout: x86_tss */
    .struct 0
    .struct regsize32
rsp_0:
    .struct rsp_0 + regsize
rsp_1:
    .struct rsp_1 + regsize
rsp_2:
    .struct rsp_2 + regsize
rsp_3:
    .struct rsp_3 + regsize
ist_null:
    .struct ist_null + regsize
ist_1:

