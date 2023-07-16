#ifndef __LUNAIX_SERIAL_H
#define __LUNAIX_SERIAL_H

#include <lunaix/types.h>

#define SERIAL_COM1 0x3f8
#define SERIAL_COM2 0x2f8

#define COM1_IRQ 4
#define COM2_IRQ 3

#define BAUD_115200 1
#define BAUD_57600 2
#define BAUD_38400 3
#define BAUD_9600 12

#define COM_RRXTX(port) (port)
#define COM_RIE(port) (port + 1)
#define COM_RCFIFO(port) (port + 2)
#define COM_RCLINE(port) (port + 3)
#define COM_RCMODEM(port) (port + 4)
#define COM_RSLINE(port) (port + 5)
#define COM_RSMODEM(port) (port + 6)

void
serial_init();

char
serial_rx_byte(ptr_t port);

void
serial_rx_buffer(ptr_t port, char* data, size_t len);

void
serial_tx_byte(ptr_t port, char data);

void
serial_tx_buffer(ptr_t port, char* data, size_t len);

void
serial_clear_fifo(ptr_t port);

void
serial_disable_irq(ptr_t port);

void
serial_enable_irq(ptr_t port);

#endif /* __LUNAIX_SERIAL_H */
