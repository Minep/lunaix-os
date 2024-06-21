// FIXME Re-design needed!!

#include <hal/serial.h>
#include <klibc/strfmt.h>
#include <lunaix/syslog.h>
#include <sdbg/gdbstub.h>
#include <sdbg/lsdbg.h>
#include <sdbg/protocol.h>

#include <lunaix/generic/isrm.h>

// #define USE_LSDBG_BACKEND

LOG_MODULE("SDBG")

volatile int debug_mode = 0;

// begin: @cmc
#define DBG_START 0x636d6340UL

// begin: @yay
#define DBG_END 0x79617940UL

static int
sdbg_serial_callback(struct serial_dev* sdev)
{
    u32_t dbg_sig = *(u32_t*)sdev->rw.buf;

    if (dbg_sig == DBG_START) {
        debug_mode = 1;
    } else if (dbg_sig == DBG_END) {
        debug_mode = 0;
    }

    // Debugger should be run later
    // TODO implement a defer execution mechanism (i.e., soft interrupt)

    return SERIAL_AGAIN;
}

void
sdbg_imm(const struct hart_state* hstate)
{
    struct exec_param* execp = hstate->execp;
    DEBUG("Quick debug mode\n");
    DEBUG("cs=%p eip=%p eax=%p ebx=%p\n",
            execp->cs,
            execp->eip,
            hstate->registers.eax,
            hstate->registers.ebx);
    DEBUG("ecx=%p edx=%p edi=%p esi=%p\n",
            hstate->registers.ecx,
            hstate->registers.edx,
            hstate->registers.edi,
            hstate->registers.esi);
    DEBUG("u.esp=%p k.esp=%p ebp=%p ps=%p\n",
            hstate->esp,
            execp->esp,
            hstate->registers.ebp,
            execp->eflags);
    DEBUG("ss=%p ds=%p es=%p fs=%p gs=%p\n",
            execp->ss,
            hstate->registers.ds,
            hstate->registers.es,
            hstate->registers.fs,
            hstate->registers.gs);
    while (1)
        ;
}

static char buf[4];

static void
__sdbg_breakpoint(const struct hart_state* hstate)
{
    gdbstub_loop(hstate);
}

void
sdbg_init()
{
    struct serial_dev* sdev = serial_get_avilable();

    if (!sdev) {
        ERROR("no serial port available\n");
        return;
    }

    kprintf("listening: %s\n", sdev->dev->name.value);

    serial_rwbuf_async(sdev, buf, 4, sdbg_serial_callback, SERIAL_RW_RX);
}