#ifndef __LUNAIX_AA64_SPSR_H
#define __LUNAIX_AA64_SPSR_H

#include <lunaix/types.h>
#include <lunaix/bits.h>

#define SPSR_EL          BITFIELD(3, 2)

#define SPSR_SP          BITFLAG(0)
#define SPSR_I           BITFLAG(7)
#define SPSR_F           BITFLAG(6)
#define SPSR_I           BITFLAG(7)
#define SPSR_AllInt      BITFLAG(13)

static inline bool
spsr_from_el0(reg_t spsr)
{
    return BITS_GET(spsr, SPSR_EL) == 0;
}

#endif /* __LUNAIX_AA64_SPSR_H */
