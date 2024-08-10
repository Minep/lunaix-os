#include <lunaix/mm/valloc.h>
#include <lunaix/status.h>

#include <usr/lunaix/serial.h>
#include <usr/lunaix/term.h>

#include "16x50.h"

struct uart16550*
uart_alloc(ptr_t base_addr)
{
    struct uart16550* uart = valloc(sizeof(struct uart16550));

    // load registers default value
    uart->cntl_save.rmc = UART_rMC_DTR | UART_rMC_RTS | UART_rMC_IEN;
    uart->cntl_save.rie = 0;

    uart->base_addr = base_addr;
    uart->base_clk  = 115200U;
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

static void
uart_set_control_mode(struct uart16550* uart, tcflag_t cflags)
{
    uart->cntl_save.rie &= ~(UART_rIE_ERBFI | UART_rIE_EDSSI);
    uart->cntl_save.rie |= (!(cflags & _CLOCAL)) * UART_rIE_EDSSI;
    uart->cntl_save.rie |= (!!(cflags & _CREAD)) * UART_rIE_ERBFI;
    uart_setie(uart);

    uart->cntl_save.rlc &= ~UART_LCR_RESET;
    uart->cntl_save.rlc |= (!!(cflags & _CSTOPB)) * UART_rLC_STOPB;
    uart->cntl_save.rlc |= (!!(cflags & _CPARENB)) * UART_rLC_PAREN;
    uart->cntl_save.rlc |= (!(cflags & _CPARODD)) * UART_rLC_PAREVN;
    uart->cntl_save.rlc |= (cflags & _CSZ_MASK) >> 2;
    uart_setlc(uart);
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
        case SERIO_SETBRDRATE:
            {
                unsigned int div, rate;

                rate = va_arg(args, speed_t);
                if (!rate) {
                    return EINVAL;
                }

                div  = uart->base_clk / va_arg(args, speed_t);
                uart_baud_divisor(uart, div);
                break;
            }
        case SERIO_SETBRDBASE:
            {
                int clk = va_arg(args, unsigned int);
                if (!clk) {
                    return EINVAL;
                }
                
                uart->base_clk = clk;
                break;
            }
        case SERIO_SETCNTRLMODE:
            uart_set_control_mode(uart, va_arg(args, tcflag_t));
            break;
        default:
            return ENOTSUP;
    }
    return 0;
}

void
uart_handle_irq_overlap(int iv, struct llist_header* ports)
{
    struct uart16550 *pos, *n;
    llist_for_each(pos, n, ports, local_ports)
    {
        int is = uart_intr_identify(pos);
        if (iv == pos->iv && (is == UART_CHR_TIMEOUT)) {
            goto done;
        }
    }

    return;

done:
    uart_handle_irq(iv, pos);
}

void
uart_handle_irq(int iv, struct uart16550 *uart)
{
    char tmpbuf[32];
    char recv;
    int i = 0;
    while ((recv = uart_read_byte(uart))) {
        tmpbuf[i++] = recv;
        if (likely(i < 32)) {
            continue;
        }

        if (!serial_accept_buffer(uart->sdev, tmpbuf, i)) {
            uart_clear_rxfifo(uart);
            return;
        }

        i = 0;
    }

    serial_accept_buffer(uart->sdev, tmpbuf, i);
    serial_accept_one(uart->sdev, 0);

    serial_end_recv(uart->sdev);
}