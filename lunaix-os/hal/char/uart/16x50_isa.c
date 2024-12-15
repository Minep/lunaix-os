#include <lunaix/device.h>
#include <lunaix/syslog.h>

#include <asm/x86_pmio.h>

#include "16x50.h"

LOG_MODULE("16x50-isa");

static DEFINE_LLIST(com_ports);

static void
com_irq_handler(irq_t irq, const struct hart_state* hstate)
{
    uart_handle_irq_overlap(irq, &com_ports);
}

int
isa16x50_create_once(struct device_def* def)
{
    int irq3 = 3, irq4 = 4;
    u16_t ioports[] = { 0x3F8, 0x2F8, 0x3E8, 0x2E8 };
    int* irqs[] = { &irq4, &irq3, &irq4, &irq3 };

    struct uart16550* uart = NULL;
    struct serial_dev* sdev;
    ptr_t base;

    // COM 1...4
    for (size_t i = 0; i < 4; i++) {

        base = ioports[i];
        uart = uart16x50_pmio_create(base);
        if (!uart) {
            WARN("port 0x%x not accessible", base);
            continue;
        }

        sdev = uart_create_serial(uart, &def->class, &com_ports, "S");
        
        int irq = *irqs[i];
        if (irq) {
            /*
             *  Since these irqs are overlapped, this particular setup is needed
             * to avoid double-bind
             */
            uart->irq = irq_declare_line(com_irq_handler, irq);
            irq_assign(irq_owning_domain(sdev->dev), uart->irq, NULL);
            *((volatile int*)irqs[i]) = 0;
        }
        
        INFO("base: 0x%x, irq=%d", base, irq);
    }

    return 0;
}