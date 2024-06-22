#ifndef __LUNAIX_GDBSTUB_H
#define __LUNAIX_GDBSTUB_H

#include <lunaix/hart_state.h>
#include <hal/serial.h>

struct gdb_state
{
    int signum;
    struct serial_dev* sdev;
    reg_t registers[GDB_CPU_NUM_REGISTERS];
};

void
gdbstub_loop(struct hart_state* hstate);

#endif /* __LUNAIX_GDBSTUB_H */
