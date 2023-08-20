#include <sys/cpu.h>
#include <lunaix/peripheral/serial.h>
#include <lunaix/syslog.h>
#include <sys/port_io.h>

LOG_MODULE("UART")

void
serial_init_port(ptr_t port)
{
    // disable interrupt, use irq instead
    port_wrbyte(COM_RIE(port), 0);

    // baud rate config (DLAB = 1)
    port_wrbyte(COM_RCLINE(port), 0x80);
    port_wrbyte(COM_RRXTX(port), BAUD_9600);
    port_wrbyte(COM_RIE(port), 0);

    // transmission size = 7bits, no parity, 1 stop bits
    port_wrbyte(COM_RCLINE(port), 0x2);

    // rx trigger level = 14, clear rx/tx buffer, enable buffer
    port_wrbyte(COM_RCFIFO(port), 0xcf);

    port_wrbyte(COM_RCMODEM(port), 0x1e);

    port_wrbyte(COM_RRXTX(port), 0xaa);

    if (port_rdbyte(COM_RRXTX(port)) != 0xaa) {
        kprintf(KWARN "port.%p: faulty\n", port);
        return;
    }

    port_wrbyte(COM_RCMODEM(port), 0xf);
    port_wrbyte(COM_RIE(port), 0x1);
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
    while (!(port_rdbyte(COM_RSLINE(port)) & 0x01))
        ;

    return port_rdbyte(COM_RRXTX(port));
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
    while (!(port_rdbyte(COM_RSLINE(port)) & 0x20))
        ;

    port_wrbyte(COM_RRXTX(port), data);
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
    port_wrbyte(COM_RIE(port), 0x0);
    port_wrbyte(COM_RCFIFO(port), 0x00);

    port_wrbyte(COM_RCFIFO(port), 0xcf);
    port_wrbyte(COM_RIE(port), 0x1);
}

void
serial_disable_irq(ptr_t port)
{
    port_wrbyte(COM_RIE(port), 0x0);
}

void
serial_enable_irq(ptr_t port)
{
    port_wrbyte(COM_RIE(port), 0x1);
}