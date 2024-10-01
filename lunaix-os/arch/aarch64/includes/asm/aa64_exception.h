#ifndef __LUNAIX_AA64_ESR_H
#define __LUNAIX_AA64_ESR_H

#ifdef __ASM__

#define EXCEPTION_SYNC      0
#define EXCEPTION_IFQ       1
#define EXCEPTION_IRQ       2
#define EXCEPTION_SERR      3

#else

#include <lunaix/bits.h>
#include <lunaix/types.h>

#define ESR_ISS2            BITS(55, 32)
#define ESR_EC              BITS(31, 26)
#define ESR_IL              BIT(25)
#define ESR_ISS             BITS(24,  0)

#define EC_UNKNOWN          0b000000
#define EC_WF               0b000001
#define EC_SIMD             0b000111
#define EC_LS64             0b001010
#define EC_BTI              0b001101
#define EC_EXEC_STATE       0b001110
#define EC_SYS_INST         0b011000

#define EC_I_ABORT          0b100000
#define EC_I_ABORT_EL       0b100001

#define EC_D_ABORT          0b100100
#define EC_D_ABORT_EL       0b100101

#define EC_PC_ALIGN         0b100010
#define EC_SP_ALIGN         0b100110

#define EC_SERROR           0b101111

static inline bool
esr_inst32(reg_t esr)
{
    return !!BITS_GET(esr, ESR_IL);
}

static inline unsigned int
esr_ec(reg_t esr)
{
    return (unsigned int)BITS_GET(esr, ESR_EC);
}

static inline reg_t
esr_iss(reg_t esr)
{
    return (reg_t)BITS_GET(esr, ESR_ISS);
}

#endif /* !__ASM__ */
#endif /* __LUNAIX_AA64_ESR_H */
