#include <arch/x86/interrupts.h>
#include <hal/apic.h>
#include <hal/cpu.h>
#include <lunaix/syslog.h>
#include <lunaix/tty/tty.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/vmm.h>

LOG_MODULE("intr")

static int_subscriber subscribers[256];

static int_subscriber fallback = (int_subscriber) 0;

void
intr_subscribe(const uint8_t vector, int_subscriber subscriber) {
    subscribers[vector] = subscriber;
}

void
intr_unsubscribe(const uint8_t vector, int_subscriber subscriber) {
    if (subscribers[vector] == subscriber) {
        subscribers[vector] = (int_subscriber) 0;
    }
}

void
intr_set_fallback_handler(int_subscriber subscribers) {
    fallback = subscribers;
}

extern x86_page_table* __kernel_ptd;

void
intr_handler(isr_param* param)
{
    // if (param->vector == LUNAIX_SYS_CALL) {
    //     kprintf(KDEBUG "%p", param->registers.esp);
    // }
    __current->intr_ctx = *param;

    cpu_lcr3(__kernel_ptd);

    // 将当前进程的页目录挂载到内核地址空间里（页目录挂载点#1），方便访问。
    vmm_mount_pd(PD_MOUNT_1, __current->page_table);

    isr_param *lparam = &__current->intr_ctx;
    
    if (lparam->vector <= 255) {
        int_subscriber subscriber = subscribers[lparam->vector];
        if (subscriber) {
            subscriber(lparam);
            goto done;
        }
    }

    if (fallback) {
        fallback(lparam);
        goto done;
    }
    
    kprint_panic("INT %u: (%x) [%p: %p] Unknown",
            lparam->vector,
            lparam->err_code,
            lparam->cs,
            lparam->eip);

done:

    // if (__current->state != PROC_RUNNING) {
    //     schedule();
    // }

    // for all external interrupts except the spurious interrupt
    //  this is required by Intel Manual Vol.3A, section 10.8.1 & 10.8.5
    if (lparam->vector >= EX_INTERRUPT_BEGIN && lparam->vector != APIC_SPIV_IV) {
        apic_done_servicing();
    }

    cpu_lcr3(__current->page_table);

    *param = __current->intr_ctx;

    return;
}