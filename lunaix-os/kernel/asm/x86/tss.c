#include <arch/x86/tss.h>
#include <lunaix/common.h>

struct x86_tss _tss = {
    .link = 0,
    .esp0 = KSTACK_START,
    .ss0  = KDATA_SEG
};

void tss_update(uint32_t ss0, uint32_t esp0) {
    _tss.esp0 = esp0;
    _tss.ss0 = ss0;
}