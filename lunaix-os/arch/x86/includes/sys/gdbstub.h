#ifndef __LUNAIX_ARCH_GDBSTUB_ARCH_H
#define __LUNAIX_ARCH_GDBSTUB_ARCH_H

#include "sys/hart.h"

enum GDB_REGISTER
{
    GDB_CPU_I386_REG_EAX = 0,
    GDB_CPU_I386_REG_ECX = 1,
    GDB_CPU_I386_REG_EDX = 2,
    GDB_CPU_I386_REG_EBX = 3,
    GDB_CPU_I386_REG_ESP = 4,
    GDB_CPU_I386_REG_EBP = 5,
    GDB_CPU_I386_REG_ESI = 6,
    GDB_CPU_I386_REG_EDI = 7,
    GDB_CPU_I386_REG_PC = 8,
    GDB_CPU_I386_REG_PS = 9,
    GDB_CPU_I386_REG_CS = 10,
    GDB_CPU_I386_REG_SS = 11,
    GDB_CPU_I386_REG_DS = 12,
    GDB_CPU_I386_REG_ES = 13,
    GDB_CPU_I386_REG_FS = 14,
    GDB_CPU_I386_REG_GS = 15,
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
