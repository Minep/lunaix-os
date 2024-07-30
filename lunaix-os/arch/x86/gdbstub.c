#include <sys/gdbstub.h>
#include <sdbg/gdbstub.h>

void
arch_gdbstub_setup_state(struct gdb_state* state, struct hart_state* hstate)
{
    /* Translate vector to signal */
    switch (hstate->execp->vector) {
        case 1:
            state->signum = 5;
            break;
        case 3:
            state->signum = 5;
            break;
        default:
            state->signum = 7;
    }
}

void
arch_gdbstub_save_regs(struct gdb_state* state, struct hart_state* hstate)
{
    /* Load Registers */
#ifndef CONFIG_ARCH_X86_64
    state->registers[GDB_CPU_I386_REG_EAX] = hstate->registers.eax;
    state->registers[GDB_CPU_I386_REG_ECX] = hstate->registers.ecx;
    state->registers[GDB_CPU_I386_REG_EDX] = hstate->registers.edx;
    state->registers[GDB_CPU_I386_REG_EBX] = hstate->registers.ebx;
    state->registers[GDB_CPU_I386_REG_ESP] = hstate->sp;
    state->registers[GDB_CPU_I386_REG_EBP] = hstate->registers.ebp;
    state->registers[GDB_CPU_I386_REG_ESI] = hstate->registers.esi;
    state->registers[GDB_CPU_I386_REG_EDI] = hstate->registers.edi;
    state->registers[GDB_CPU_I386_REG_PC] = hstate->execp->eip;
    state->registers[GDB_CPU_I386_REG_CS] = hstate->execp->cs;
    state->registers[GDB_CPU_I386_REG_PS] = hstate->execp->eflags;
    state->registers[GDB_CPU_I386_REG_SS] = hstate->execp->ss;
    state->registers[GDB_CPU_I386_REG_DS] = hstate->registers.ds;
    state->registers[GDB_CPU_I386_REG_ES] = hstate->registers.es;
    state->registers[GDB_CPU_I386_REG_FS] = hstate->registers.fs;
    state->registers[GDB_CPU_I386_REG_GS] = hstate->registers.gs;
#else
    // TODO
#endif
}

void
arch_gdbstub_restore_regs(struct gdb_state* state, struct hart_state* hstate)
{
    /* Restore Registers */
#ifndef CONFIG_ARCH_X86_64
    hstate->registers.eax = state->registers[GDB_CPU_I386_REG_EAX];
    hstate->registers.ecx = state->registers[GDB_CPU_I386_REG_ECX];
    hstate->registers.edx = state->registers[GDB_CPU_I386_REG_EDX];
    hstate->registers.ebx = state->registers[GDB_CPU_I386_REG_EBX];
    hstate->sp = state->registers[GDB_CPU_I386_REG_ESP];
    hstate->registers.ebp = state->registers[GDB_CPU_I386_REG_EBP];
    hstate->registers.esi = state->registers[GDB_CPU_I386_REG_ESI];
    hstate->registers.edi = state->registers[GDB_CPU_I386_REG_EDI];
    hstate->execp->eip = state->registers[GDB_CPU_I386_REG_PC];
    hstate->execp->cs = state->registers[GDB_CPU_I386_REG_CS];
    hstate->execp->eflags = state->registers[GDB_CPU_I386_REG_PS];
    hstate->execp->ss = state->registers[GDB_CPU_I386_REG_SS];
    hstate->registers.ds = state->registers[GDB_CPU_I386_REG_DS];
    hstate->registers.es = state->registers[GDB_CPU_I386_REG_ES];
    hstate->registers.fs = state->registers[GDB_CPU_I386_REG_FS];
    hstate->registers.gs = state->registers[GDB_CPU_I386_REG_GS];
#else
    // TODO
#endif
}


int
gdb_sys_continue(struct gdb_state* state)
{
    state->registers[GDB_CPU_I386_REG_PS] &= ~(1 << 8);
    return 0;
}

int
gdb_sys_step(struct gdb_state* state)
{
    state->registers[GDB_CPU_I386_REG_PS] |= 1 << 8;
    return 0;
}