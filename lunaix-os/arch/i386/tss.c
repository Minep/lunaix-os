#include <lunaix/common.h>
#include <lunaix/process.h>
#include <sys/x86_isa.h>

volatile struct x86_tss _tss = { .link = 0,
                                 .esp0 = KSTACK_TOP,
                                 .ss0 = KDATA_SEG };