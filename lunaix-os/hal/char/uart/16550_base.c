#include <lunaix/mm/valloc.h>
#include <lunaix/status.h>

#include <usr/lunaix/serial.h>

#include "16550.h"

struct uart16550*
uart_alloc(ptr_t base_addr)
{
    struct uart16550* uart = valloc(sizeof(struct uart16550));

    // load registers default value
    uart->cntl_save.rmc = UART_rMC_DTR | UART_rMC_RTS | UART_rMC_IEN;
    uart->cntl_save.rie = 0;

    uart->base_addr = base_addr;
    return uart;
}

void
uart_free(struct uart16550* uart)
{
    vfree(uart);
}

int
uart_general_tx(struct serial_dev* sdev, u8_t* data, size_t len)
{
    struct uart16550* uart = UART16550(sdev);

    size_t i = 0;
    while (i < len) {
        while (!uart_can_transmit(uart))
            ;
        uart_write_byte(uart, data[i++]);
    }

    serial_end_xmit(sdev, len);

    return RXTX_DONE;
}

int
uart_general_exec_cmd(struct serial_dev* sdev, u32_t req, va_list args)
{
    struct uart16550* uart = UART16550(sdev);
    switch (req) {
        case SERIO_RXEN:
            uart_setie(uart);
            break;
        case SERIO_RXDA:
            uart_clrie(uart);
            break;
        default:
            return ENOTSUP;
    }
    return 0;
}

void
uart_general_irq_handler(int iv, struct llist_header* ports)
{
    char tmpbuf[32];
    struct uart16550 *pos, *n;
    llist_for_each(pos, n, ports, local_ports)
    {
        int is = uart_intr_identify(pos);
        if (iv == pos->iv && is == UART_DATA_OK) {
            break;
        }
    }

    if (!pos) {
        return;
    }

    char recv;
    int i = 0;
    while ((recv = uart_read_byte(pos))) {
        tmpbuf[i++] = recv;
        if (likely(i < 32)) {
            continue;
        }

        if (!serial_accept_buffer(pos->sdev, tmpbuf, i)) {
            uart_clear_rxfifo(pos);
            return;
        }

        i = 0;
    }

    serial_accept_buffer(pos->sdev, tmpbuf, i);
    serial_end_recv(pos->sdev);
}
