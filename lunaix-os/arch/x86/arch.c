#include <hal/hwtimer.h>

#include <lunaix/spike.h>
#include <lunaix/process.h>

#include "asm/x86.h"
#include "asm/hart.h"

#include "hal/apic_timer.h"

void
exception_init()
{
    exception_install_handler();
}

void
arch_preinit()
{
    exception_init();
}

void
update_tss()
{
    extern struct x86_tss _tss;
#ifdef CONFIG_ARCH_X86_64
    _tss.rsps[0] = (ptr_t)current_thread->hstate;
#else
    _tss.esp0 = (u32_t)current_thread->hstate;
#endif
}