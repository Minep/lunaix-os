/**
 * @file 16550_pmio.c
 * @author Lunaixsky (lunaxisky@qq.com)
 * @brief 16550 UART with port mapped IO
 * @version 0.1
 * @date 2023-08-30
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <lunaix/device.h>
#include <lunaix/isrm.h>

#include <sys/port_io.h>

#include "16550.h"

#define DELAY 50

static DEFINE_LLIST(com_ports);

static u32_t
com_regread(struct uart16550* uart, ptr_t regoff)
{
    u8_t val = port_rdbyte(uart->base_addr + regoff);
    port_delay(DELAY);

    return (u32_t)val;
}

static void
com_regwrite(struct uart16550* uart, ptr_t regoff, u32_t val)
{
    port_wrbyte(uart->base_addr + regoff, (u8_t)val);
    port_delay(DELAY);
}

static void
com_irq_handler(const isr_param* param)
{
    uart_general_irq_handler(param->execp->vector, &com_ports);
}

static int
upiom_init(struct device_def* def)
{
    int irq3 = 3, irq4 = 4;
    u16_t ioports[] = { 0x3F8, 0x2F8, 0x3E8, 0x2E8 };
    int* irqs[] = { &irq4, &irq3, &irq4, &irq3 };

    struct uart16550* uart = NULL;

    // COM 1...4
    for (size_t i = 0; i < 4; i++) {
        if (!uart) {
            uart = uart_alloc(ioports[i]);
            uart->read_reg = com_regread;
            uart->write_reg = com_regwrite;
        } else {
            uart->base_addr = ioports[i];
        }

        if (!uart_testport(uart, 0xe3)) {
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

        uart_enable_fifo(uart, UART_FIFO8);
        llist_append(&com_ports, &uart->local_ports);

        struct serial_dev* sdev = serial_create(&def->class);
        sdev->backend = uart;
        sdev->write = uart_general_tx;
        sdev->exec_cmd = uart_general_exec_cmd;

        uart->sdev = sdev;

        uart_setup(uart);
        uart_setie(uart);
        uart = NULL;
    }

    if (uart) {
        uart_free(uart);
    }

    return 0;
}

static struct device_def uart_pmio_def = {
    .class = DEVCLASS(DEVIF_SOC, DEVFN_CHAR, DEV_UART16550),
    .name = "16550 Generic UART (I/O)",
    .init = upiom_init
};
EXPORT_DEVICE(uart16550_pmio, &uart_pmio_def, load_earlystage);