#include <arch/x86/interrupts.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/common.h>
#include <lunaix/syslog.h>

static void kprintf(const char* fmt, ...) { va_list args; va_start(args, fmt); __kprintf("PFAULT", fmt, args); va_end(args); }

extern void __print_panic_msg(const char* msg, const isr_param* param);

void
intr_routine_page_fault (const isr_param* param) 
{
    void* pg_fault_ptr = cpu_rcr2();
    if (!pg_fault_ptr) {
        __print_panic_msg("Null pointer reference", param);
        goto done;
    }

    kprintf("%p", pg_fault_ptr);
    __print_panic_msg("Page fault", param);

done:
    while(1);
}