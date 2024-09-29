#ifndef __LUNAIX_AA64_SYSINST_H
#define __LUNAIX_AA64_SYSINST_H

#include "aa64_asm.h"

#define tlbi_alle1      __sr_encode(1, 4, 8, 7, 4)
#define tlbi_aside1     __sr_encode(1, 0, 8, 7, 2)
#define tlbi_rvaae1     __sr_encode(1, 0, 8, 6, 3)
#define tlbi_rvae1      __sr_encode(1, 0, 8, 6, 1)
#define tlbi_vaae1      __sr_encode(1, 0, 8, 7, 3)
#define tlbi_vae1       __sr_encode(1, 0, 8, 7, 1)

#define sys_a0(op)    \
    ({  asm ("sys " stringify(op)); })

#define sys_a1(op, xt)    \
    ({  asm ("sys " stringify(op) ", %0" :: "r"(xt)); })

#define sysl(op)    \
    ({  unsigned long _x;                               \
        asm ("sysl %0, " stringify(op):"=r"(_x));       \
        _x;                                             \
    })

#endif /* __LUNAIX_AA64_SYSINST_H */
