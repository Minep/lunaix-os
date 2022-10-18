#ifndef __LUNAIX_LSDBG_H
#define __LUNAIX_LSDBG_H

#include <arch/x86/interrupts.h>

#define SDBG_CLNT_HI 0x10
#define SDBG_CLNT_QUIT 0xff
#define SDBG_CLNT_RREG 0x11
#define SDBG_CLNT_STEP 0x12
#define SDBG_CLNT_CONT 0x13
#define SDBG_CLNT_BRKP 0x14

#define SDBG_SVER_MSG 0xa1
#define SDBG_SVER_WHATNEXT 0xa2

#define SDBG_STATE_START 0
#define SDBG_STATE_INSESSION 1
#define SDBG_STATE_WAIT_BRK 2

void
lunaix_sdbg_loop(isr_param* param);

#endif /* __LUNAIX_LSDBG_H */
