#include <lunaix/device.h>
#include <lunaix/syslog.h>

#include <asm/x86_pmio.h>
#include <asm/x86_isrm.h>

#include "16x50.h"

LOG_MODULE("16x50-isa");

static DEFINE_LLIST(com_ports);

static void
com_irq_handler(const struct hart_state* hstate)
{
    int vector = hart_vector_stamp(hstate);
    uart_handle_irq_overlap(vector, &com_ports);
}

int
isa16x50_create_once(struct device_def* def)
{
    int irq3 = 3, irq4 = 4;
    u16_t ioports[] = { 0x3F8, 0x2F8, 0x3E8, 0x2E8 };
    int* irqs[] = { &irq4, &irq3, &irq4, &irq3 };

    struct uart16550* uart = NULL;
    ptr_t base;

    // COM 1...4
    for (size_t i = 0; i < 4; i++) {

        base = ioports[i];
        uart = uart16x50_pmio_create(base);
        if (!uart) {
            WARN("port 0x%x not accessible", base);
            continue;
        }

        int irq = *irqs[i];
        if (irq) {
            /*
             *  Since these irqs are overlapped, this particular setup is needed
             * to avoid double-bind
             */
            uart->iv = isrm_bindirq(irq, com_irq_handler);
            *((volatile int*)irqs[i]) = 0;
        }

        INFO("base: 0x%x, irq=%d", 
                base, irq, uart->iv);

        uart_create_serial(uart, &def->class, &com_ports, "S");
    }

    return 0;
}