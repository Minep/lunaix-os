#include <arch/x86/tss.h>
#include <lunaix/common.h>

volatile struct x86_tss _tss = { .link = 0,
                                 .esp0 = KSTACK_TOP,
                                 .ss0 = KDATA_SEG };

void
tss_update_esp(u32_t esp0)
{
    _tss.esp0 = esp0;
}