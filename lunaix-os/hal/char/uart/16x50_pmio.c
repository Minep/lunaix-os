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
#include <asm/isrm.h>

#include <asm/x86_pmio.h>

#include "16x50.h"

#define DELAY 50

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


struct uart16550*
uart16x50_pmio_create(ptr_t base)
{
    struct uart16550* uart;

    uart = uart_alloc(base);
    uart->read_reg = com_regread;
    uart->write_reg = com_regwrite;

    if (!uart_testport(uart, 0xe3)) {
        uart_free(uart);
        return NULL;
    }

    uart_enable_fifo(uart, UART_FIFO8);

    return uart;
}
