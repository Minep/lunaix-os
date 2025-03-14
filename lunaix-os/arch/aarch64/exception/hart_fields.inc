    .struct 0
reg_xn:
    .struct reg_xn + 8 * 29
reg_fp:
    .struct reg_fp + 8
reg_lr:
    .struct reg_lr + 8
reg_end:

    .struct 0
execp_parent:
    .struct execp_parent + 8
execp_spsr:
    .struct execp_spsr + 8
execp_link:
    .struct execp_link + 8
execp_spel0_saved:
    .struct execp_spel0_saved + 8
execp_rsvd:
    .struct execp_rsvd + 8
execp_syndrome:
    .struct execp_syndrome + 8
execp_end:

    .struct 0
hart_depth:
    .struct hart_depth + 8
hart_regs:
    .struct hart_regs + reg_end
hart_execp:
    .struct hart_execp + execp_end
hart_end:

    .struct 0
sig_num:
    .struct sig_pend + 4
sigact:
    .struct sigact + 8
sighand:
    .struct sighand + 8
saved_hstate:
    .struct saved_hstate + 8
sig_end: