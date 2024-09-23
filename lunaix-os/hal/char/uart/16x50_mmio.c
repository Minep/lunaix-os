#include <lunaix/device.h>
#include <asm/isrm.h>
#include <lunaix/mm/mmio.h>

#include "16x50.h"

static u32_t
uart_mmio_regread(struct uart16550* uart, ptr_t regoff)
{
    return (u32_t)(*(u8_t*)(uart->base_addr + regoff));
}

static void
uart_mmio_regwrite(struct uart16550* uart, ptr_t regoff, u32_t val)
{
    *(u8_t*)(uart->base_addr + regoff) = (u8_t)val;
}

struct uart16550*
uart16x50_mmio_create(ptr_t base, ptr_t size)
{
    ptr_t mmio_base;
    struct uart16550* uart;

    base = ioremap(base, size);
    uart = uart_alloc(base);
    uart->read_reg = uart_mmio_regread;
    uart->write_reg = uart_mmio_regwrite;

    if (!uart_testport(uart, 0xe3)) {
        iounmap(base, size);
        uart_free(uart);
        return NULL;
    }

    uart_enable_fifo(uart, UART_FIFO8);

    return uart;
}