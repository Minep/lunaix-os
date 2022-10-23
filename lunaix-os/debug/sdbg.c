#include <hal/acpi/acpi.h>
#include <hal/ioapic.h>
#include <klibc/stdio.h>
#include <lunaix/isrm.h>
#include <lunaix/lxconsole.h>
#include <lunaix/peripheral/serial.h>
#include <lunaix/syslog.h>
#include <sdbg/gdbstub.h>
#include <sdbg/lsdbg.h>
#include <sdbg/protocol.h>

// #define USE_LSDBG_BACKEND

LOG_MODULE("SDBG")

volatile int debug_mode = 0;

void
sdbg_loop(const isr_param* param)
{
    // This is importnat, because we will handle any subsequent RX/TX in a
    // synchronized way. And we don't want these irq queue up at our APIC and
    // confuse the CPU after ACK with APIC.
    serial_disable_irq(SERIAL_COM1);
    if (param->vector == 1 || param->vector == 3) {
        goto cont;
    }

    if (!debug_mode) {
        // Oh... C.M.C. is about to help the debugging!
        if (serial_rx_byte(SERIAL_COM1) != '@') {
            goto done;
        }
        if (serial_rx_byte(SERIAL_COM1) != 'c') {
            goto done;
        }
        if (serial_rx_byte(SERIAL_COM1) != 'm') {
            goto done;
        }
        if (serial_rx_byte(SERIAL_COM1) != 'c') {
            goto done;
        }
        debug_mode = 1;
    } else {
        if (serial_rx_byte(SERIAL_COM1) != '@') {
            goto cont;
        }
        if (serial_rx_byte(SERIAL_COM1) != 'y') {
            goto cont;
        }
        if (serial_rx_byte(SERIAL_COM1) != 'a') {
            goto cont;
        }
        if (serial_rx_byte(SERIAL_COM1) != 'y') {
            goto cont;
        }
        debug_mode = 0;
        goto done;
    }

cont:
    kprint_dbg(" DEBUG");
#ifdef USE_LSDBG_BACKEND
    lunaix_sdbg_loop(param);
#else
    gdbstub_loop(param);
#endif
    console_flush();

done:
    serial_enable_irq(SERIAL_COM1);
}

void
sdbg_imm(const isr_param* param)
{
    kprintf(KDEBUG "Quick debug mode\n");
    kprintf(KDEBUG "cs=%p eip=%p eax=%p ebx=%p\n",
            param->cs,
            param->eip,
            param->registers.eax,
            param->registers.ebx);
    kprintf(KDEBUG "ecx=%p edx=%p edi=%p esi=%p\n",
            param->registers.ecx,
            param->registers.edx,
            param->registers.edi,
            param->registers.esi);
    kprintf(KDEBUG "u.esp=%p k.esp=%p ebp=%p ps=%p\n",
            param->registers.esp,
            param->esp,
            param->registers.ebp,
            param->eflags);
    kprintf(KDEBUG "ss=%p ds=%p es=%p fs=%p gs=%p\n",
            param->ss,
            param->registers.ds,
            param->registers.es,
            param->registers.fs,
            param->registers.gs);
    console_flush();
    while (1)
        ;
}
void
sdbg_init()
{
    isrm_bindiv(INSTR_DEBUG, sdbg_loop); // #DB
    isrm_bindiv(INSTR_BREAK, sdbg_loop); // #BRK

    isrm_bindirq(COM1_IRQ, sdbg_loop);
}