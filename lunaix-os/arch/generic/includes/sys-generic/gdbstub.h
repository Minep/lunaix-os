#ifndef __LUNAIX_ARCH_GDBSTUB_ARCH_H
#define __LUNAIX_ARCH_GDBSTUB_ARCH_H

#include "asm/hart.h"

enum GDB_REGISTER
{
    // TODO add your registers
    GDB_CPU_NUM_REGISTERS
};

struct gdb_state;

void
arch_gdbstub_setup_state(struct gdb_state* state, struct hart_state* hstate);

void
arch_gdbstub_save_regs(struct gdb_state* state, struct hart_state* hstate);

void
arch_gdbstub_restore_regs(struct gdb_state* state, struct hart_state* hstate);

int
gdb_sys_continue(struct gdb_state* state);

int
gdb_sys_step(struct gdb_state* state);

#endif /* __LUNAIX_ARCH_GDBSTUB_ARCH_H */