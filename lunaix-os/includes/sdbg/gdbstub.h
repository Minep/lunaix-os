#ifndef __LUNAIX_GDBSTUB_H
#define __LUNAIX_GDBSTUB_H

#include <lunaix/hart_state.h>

void
gdbstub_loop(struct hart_state* hstate);

#endif /* __LUNAIX_GDBSTUB_H */
