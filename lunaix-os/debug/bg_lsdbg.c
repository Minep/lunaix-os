#include <klibc/stdio.h>
#include <lunaix/peripheral/serial.h>
#include <lunaix/syslog.h>
#include <sdbg/lsdbg.h>

#define COM SERIAL_COM1

LOG_MODULE("DBG");

static volatile int sdbg_state = 0;

void
sdbg_printf(char* fmt, ...)
{
    char buf[256];
    va_list l;

    va_start(l, fmt);

    size_t len = __ksprintf_internal(buf, fmt, 256, l);
    serial_tx_buffer(COM, buf, len);

    va_end(l);
}

void
lunaix_sdbg_loop(isr_param* param)
{
    u8_t c;

    if (sdbg_state == SDBG_STATE_WAIT_BRK) {
        (param)->execp->eflags &= ~(1 << 8);
        sdbg_state = SDBG_STATE_INSESSION;
        sdbg_printf("[%p:%p] Break point reached.\n",
                    param->execp->cs,
                    param->execp->eip);
    }

    while (1) {
        c = (u8_t)serial_rx_byte(SERIAL_COM1);
        if (c == SDBG_CLNT_QUIT) {
            sdbg_state = SDBG_STATE_START;
            break;
        }

        switch (c) {
            case SDBG_CLNT_HI:
                if (sdbg_state == SDBG_STATE_START) {
                    sdbg_printf("[%p:%p] Session started.\n",
                                param->execp->cs,
                                param->execp->eip);
                    sdbg_state = SDBG_STATE_INSESSION;
                } else {
                    sdbg_printf("[%p:%p] Session resumed.\n",
                                param->execp->cs,
                                param->execp->eip);
                }
                break;
            case SDBG_CLNT_RREG:

                serial_tx_buffer(SERIAL_COM1, (char*)param, sizeof(isr_param));
                break;
            case SDBG_CLNT_STEP:
                ((isr_param*)param)->execp->eflags |= (1 << 8); // set TF flags
                sdbg_state = SDBG_STATE_WAIT_BRK;
                return;
            case SDBG_CLNT_BRKP:
                // the break point address
                // serial_rx_buffer(SERIAL_COM1, buffer, sizeof(ptr_t));

                // asm("movl %0, %%dr0" ::"r"(*((ptr_t*)buffer)));

                sdbg_state = SDBG_STATE_WAIT_BRK;
                return;
            case SDBG_CLNT_CONT:
                return;
            default:
                break;
        }
    }
}