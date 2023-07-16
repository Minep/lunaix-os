#include <hal/cpu.h>
#include <hal/io.h>
#include <lunaix/peripheral/serial.h>
#include <lunaix/syslog.h>

LOG_MODULE("UART")

void
serial_init_port(ptr_t port)
{
    // disable interrupt, use irq instead
    io_outb(COM_RIE(port), 0);

    // baud rate config (DLAB = 1)
    io_outb(COM_RCLINE(port), 0x80);
    io_outb(COM_RRXTX(port), BAUD_9600);
    io_outb(COM_RIE(port), 0);

    // transmission size = 7bits, no parity, 1 stop bits
    io_outb(COM_RCLINE(port), 0x2);

    // rx trigger level = 14, clear rx/tx buffer, enable buffer
    io_outb(COM_RCFIFO(port), 0xcf);

    io_outb(COM_RCMODEM(port), 0x1e);

    io_outb(COM_RRXTX(port), 0xaa);

    if (io_inb(COM_RRXTX(port)) != 0xaa) {
        kprintf(KWARN "port.%p: faulty\n", port);
        return;
    }

    io_outb(COM_RCMODEM(port), 0xf);
    io_outb(COM_RIE(port), 0x1);
    kprintf("port.%p: ok\n", port);
}

void
serial_init()
{
    cpu_disable_interrupt();
    serial_init_port(SERIAL_COM1);
    serial_init_port(SERIAL_COM2);
    cpu_enable_interrupt();
}

char
serial_rx_byte(ptr_t port)
{
    while (!(io_inb(COM_RSLINE(port)) & 0x01))
        ;

    return io_inb(COM_RRXTX(port));
}

void
serial_rx_buffer(ptr_t port, char* data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        data[i] = serial_rx_byte(port);
    }
}

void
serial_tx_byte(ptr_t port, char data)
{
    while (!(io_inb(COM_RSLINE(port)) & 0x20))
        ;

    io_outb(COM_RRXTX(port), data);
}

void
serial_tx_buffer(ptr_t port, char* data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        serial_tx_byte(port, data[i]);
    }
}

void
serial_clear_fifo(ptr_t port)
{
    io_outb(COM_RIE(port), 0x0);
    io_outb(COM_RCFIFO(port), 0x00);

    io_outb(COM_RCFIFO(port), 0xcf);
    io_outb(COM_RIE(port), 0x1);
}

void
serial_disable_irq(ptr_t port)
{
    io_outb(COM_RIE(port), 0x0);
}

void
serial_enable_irq(ptr_t port)
{
    io_outb(COM_RIE(port), 0x1);
}