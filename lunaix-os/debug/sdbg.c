#include <hal/acpi/acpi.h>
#include <hal/ioapic.h>
#include <klibc/stdio.h>
#include <lunaix/lxconsole.h>
#include <lunaix/peripheral/serial.h>
#include <lunaix/syslog.h>
#include <sdbg/gdbstub.h>
#include <sdbg/lsdbg.h>
#include <sdbg/protocol.h>

// #define USE_LSDBG_BACKEND

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

extern uint8_t
ioapic_get_irq(acpi_context* acpi_ctx, uint8_t old_irq);

void
sdbg_init()
{
    intr_subscribe(UART_COM1, sdbg_loop);
    intr_subscribe(INSTR_DEBUG, sdbg_loop); // #DB

    acpi_context* acpi_ctx = acpi_get_context();
    uint8_t irq = ioapic_get_irq(acpi_ctx, COM1_IRQ);
    ioapic_redirect(irq, UART_COM1, 0, IOAPIC_DELMOD_FIXED);
}