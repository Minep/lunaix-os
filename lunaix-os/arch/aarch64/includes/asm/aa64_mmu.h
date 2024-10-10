#ifndef __LUNAIX_AA64_MMU_H
#define __LUNAIX_AA64_MMU_H

#include "aa64_asm.h"

#if    defined(CONFIG_AA64_PAGE_GRAN_4K)
#define _MMU_TG    0b01
#elif  defined(CONFIG_AA64_PAGE_GRAN_16K)
#define _MMU_TG    0b10
#elif  defined(CONFIG_AA64_PAGE_GRAN_64K)
#define _MMU_TG    0b11
#endif

#if CONFIG_AA_OA_SIZE == 52
#define _MMU_USE_OA52
#endif


#define TCR_DS              (1UL << 59)
#define TCR_E0PD1           (1UL << 56)
#define TCR_E0PD0           (1UL << 55)
#define TCR_TBID1           (1UL << 52)
#define TCR_TBID0           (1UL << 51)
#define TCR_HPD1            (1UL << 42)
#define TCR_HPD0            (1UL << 41)
#define TCR_HD              (1UL << 40)
#define TCR_HA              (1UL << 39)
#define TCR_TBI1            (1UL << 38)
#define TCR_TBI0            (1UL << 37)
#define TCR_AS              (1UL << 36)

#define TCR_G4K             (0b01)
#define TCR_G16K            (0b10)
#define TCR_G64K            (0b11)

#define TCR_SHNS            (0b00)
#define TCR_SHOS            (0b10)
#define TCR_SHIS            (0b11)

#define TCR_TG1(g)          (((g) & 0b11) << 30)
#define TCR_TG0(g)          (((g) & 0b11) << 14)

#define TCR_T1SZ(sz)        (((sz) & 0b111111) << 16)
#define TCR_T0SZ(sz)        (((sz) & 0b111111))

#define TCR_EPD1            (1UL << 23)
#define TCR_EPD0            (1UL << 7)
#define TCR_A1              (1UL << 22)

#endif /* __LUNAIX_AA64_MMU_H */
