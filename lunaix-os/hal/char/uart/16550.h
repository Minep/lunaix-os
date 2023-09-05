#ifndef __LUNAIX_16550_H
#define __LUNAIX_16550_H

#include <hal/serial.h>
#include <lunaix/types.h>

#define UART_rRxTX 0
#define UART_rIE 1
#define UART_rII 2
#define UART_rFC 2
#define UART_rLC 3
#define UART_rMC 4
#define UART_rLS 5
#define UART_rMS 6
#define UART_rSC 7
#define UART_rDLL 0
#define UART_rDLM 1

#define UART_INTRX 0x1
#define UART_DLAB (1 << 7)
#define UART_LOOP (1 << 4)

#define UART_rIE_ERBFI 1
#define UART_rIE_ETBEI (1 << 1)
#define UART_rIE_ELSI (1 << 2)
#define UART_rIE_EDSSI (1 << 3)

#define UART_rLS_THRE (1 << 5)
#define UART_rLS_DR 1
#define UART_rLS_BI (1 << 4)

#define UART_rII_FIFOEN (0b11 << 6)
#define UART_rII_ID 0b1111

#define UART_rFC_EN 1
#define UART_rFC_DMA1 (1 << 3)
#define UART_rFC_XMIT_RESET (1 << 2)
#define UART_rFC_RCVR_RESET (1 << 1)

#define UART_rMC_DTR 1
#define UART_rMC_RTS (1 << 1)
#define UART_rMC_IEN (1 << 3)

#define UART_FIFO1 0b00
#define UART_FIFO4 0b01
#define UART_FIFO8 0b10
#define UART_FIFO14 0b11

#define UART_NO_INTR 0b0001
#define UART_LINE_UDPDATE 0b0110
#define UART_DATA_OK 0b0100
#define UART_CHR_TIMEOUT 0b1100
#define UART_SENT_ALL 0b0010
#define UART_MODEM_UPDATE 0b0000

struct uart16550
{
    struct llist_header local_ports;
    struct serial_dev* sdev;
    ptr_t base_addr;
    int iv;

    struct
    {
        u8_t rie;
        u8_t rfc;
        u8_t rmc;
    } cntl_save;

    u32_t (*read_reg)(struct uart16550* uart, ptr_t regoff);
    void (*write_reg)(struct uart16550* uart, ptr_t regoff, u32_t val);
};

#define UART16550(sdev) ((struct uart16550*)(sdev)->backend)

static inline void
uart_setup(struct uart16550* uart)
{
    uart->write_reg(uart, UART_rMC, uart->cntl_save.rmc);
    uart->write_reg(uart, UART_rIE, uart->cntl_save.rie);
}

static inline void
uart_clrie(struct uart16550* uart)
{
    uart->cntl_save.rie = uart->read_reg(uart, UART_rIE);
    uart->write_reg(uart, UART_rIE, 0);
}

static inline void
uart_setie(struct uart16550* uart)
{
    uart->write_reg(uart, UART_rIE, uart->cntl_save.rie | 1);
}

struct uart16550*
uart_alloc(ptr_t base_addr);

void
uart_free(struct uart16550*);

static inline int
uart_baud_divisor(struct uart16550* uart, int div)
{
    u32_t rlc = uart->read_reg(uart, UART_rLC);

    uart->write_reg(uart, UART_rLC, UART_DLAB | rlc);
    u8_t ls = (div & 0xff), ms = (div & 0xff00) >> 8;

    uart->write_reg(uart, UART_rLS, ls);
    uart->write_reg(uart, UART_rMS, ms);

    uart->write_reg(uart, UART_rLC, rlc & ~UART_DLAB);

    return 0;
}

static inline int
uart_testport(struct uart16550* uart, char test_code)
{
    u32_t rmc = uart->cntl_save.rmc;
    uart->write_reg(uart, UART_rMC, rmc | UART_LOOP);

    uart->write_reg(uart, UART_rRxTX, test_code);

    u32_t result = (char)uart->read_reg(uart, UART_rRxTX) == test_code;
    uart->write_reg(uart, UART_rMC, rmc & ~UART_LOOP);

    return result;
}

static inline int
uart_pending_data(struct uart16550* uart)
{
    return uart->read_reg(uart, UART_rLS) & UART_rLS_DR;
}

static inline int
uart_can_transmit(struct uart16550* uart)
{
    return uart->read_reg(uart, UART_rLS) & UART_rLS_THRE;
}

/**
 * @brief End of receiving
 *
 * @param uart
 * @return int
 */
static inline int
uart_eorcv(struct uart16550* uart)
{
    return uart->read_reg(uart, UART_rLS) & UART_rLS_BI;
}

static inline int
uart_enable_fifo(struct uart16550* uart, int trig_lvl)
{
    uart->cntl_save.rfc =
      UART_rFC_EN | ((trig_lvl & 0b11) << 6) | UART_rFC_DMA1;
    uart->write_reg(uart, UART_rFC, uart->cntl_save.rfc);

    return uart->read_reg(uart, UART_rII) & UART_rII_FIFOEN;
}

static inline void
uart_clear_rxfifo(struct uart16550* uart)
{
    uart->write_reg(uart, UART_rFC, uart->cntl_save.rfc | UART_rFC_RCVR_RESET);
}

static inline void
uart_clear_txfifo(struct uart16550* uart)
{
    uart->write_reg(uart, UART_rFC, uart->cntl_save.rfc | UART_rFC_XMIT_RESET);
}

static inline void
uart_clear_fifo(struct uart16550* uart)
{
    u32_t rfc = uart->cntl_save.rfc | UART_rFC_XMIT_RESET | UART_rFC_RCVR_RESET;
    uart->write_reg(uart, UART_rFC, rfc);
}

static inline int
uart_intr_identify(struct uart16550* uart)
{
    u32_t rii = uart->read_reg(uart, UART_rII);
    return (rii & UART_rII_ID);
}

static inline u8_t
uart_read_byte(struct uart16550* uart)
{
    return (u8_t)uart->read_reg(uart, UART_rRxTX);
}

static inline void
uart_write_byte(struct uart16550* uart, u8_t val)
{
    uart->write_reg(uart, UART_rRxTX, val);
}

int
uart_general_exec_cmd(struct serial_dev* sdev, u32_t req, va_list args);

int
uart_general_tx(struct serial_dev* sdev, u8_t* data, size_t len);

void
uart_general_irq_handler(int iv, struct llist_header* ports);

#endif /* __LUNAIX_16550_H */
