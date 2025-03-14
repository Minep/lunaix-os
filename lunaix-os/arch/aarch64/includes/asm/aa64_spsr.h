#ifndef __LUNAIX_AA64_SPSR_H
#define __LUNAIX_AA64_SPSR_H

#include <lunaix/types.h>
#include <lunaix/bits.h>

#define SPSR_EL          BITFIELD(3, 2)

#define SPSR_SP          BITFLAG(0)
#define SPSR_F           BITFLAG(6)
#define SPSR_I           BITFLAG(7)
#define SPSR_A           BITFLAG(8)
#define SPSR_AllInt      BITFLAG(13)
#define SPSR_PAN         BITFLAG(22)
#define SPSR_UAO         BITFLAG(23)

#define SPSR_EL0_preset  (BITS_AT(0, SPSR_EL) | SPSR_SP)
#define SPSR_EL1_preset  (BITS_AT(1, SPSR_EL) | SPSR_SP | SPSR_UAO)

static inline bool
spsr_from_el0(reg_t spsr)
{
    return BITS_GET(spsr, SPSR_EL) == 0;
}

#endif /* __LUNAIX_AA64_SPSR_H */
