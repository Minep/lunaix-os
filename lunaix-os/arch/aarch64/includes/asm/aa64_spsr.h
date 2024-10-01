#ifndef __LUNAIX_AA64_SPSR_H
#define __LUNAIX_AA64_SPSR_H

#include <lunaix/types.h>
#include <lunaix/bits.h>

#define SPSR_EL          BITS(3, 2)
#define SPSR_SP          BIT(0)

static inline bool
spsr_from_el0(reg_t spsr)
{
    return BITS_GET(spsr, SPSR_EL) == 0;
}

#endif /* __LUNAIX_AA64_SPSR_H */
