#ifndef __LUNAIX_GDBSTUB_H
#define __LUNAIX_GDBSTUB_H

#include <lunaix/pcontext.h>

void
gdbstub_loop(isr_param* param);

#endif /* __LUNAIX_GDBSTUB_H */
