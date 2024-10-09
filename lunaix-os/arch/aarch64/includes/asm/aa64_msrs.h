#ifndef __LUNAIX_AA64_MSRS_H
#define __LUNAIX_AA64_MSRS_H

#include "aa64_asm.h"

#define SCTLR_EL1       __sr_encode(3, 0,  1,  0, 0)
#define TCR_EL1         __sr_encode(3, 0,  2,  0, 2)
#define TTBR0_EL1       __sr_encode(3, 0,  2,  0, 0)
#define TTBR1_EL1       __sr_encode(3, 0,  2,  0, 1)
#define VBAR_EL1        __sr_encode(3, 0, 12,  0, 1)
#define CurrentEL       __sr_encode(3, 0,  4,  2, 2)
#define ELR_E1          __sr_encode(3, 0,  4,  0, 1)
#define SPSel           __sr_encode(3, 0,  4,  2, 0)
#define SPSR_EL1        __sr_encode(3, 0,  4,  0, 0)
#define DAIF_EL1        __sr_encode(3, 3,  4,  2, 1)
#define ALLINT_EL1      __sr_encode(3, 0,  4,  3, 0)
#define SP_EL0          __sr_encode(3, 0,  4,  1, 0)
#define SP_EL1          __sr_encode(3, 4,  4,  1, 0)

#ifndef __ASM__
#define read_sysreg(reg)                                    \
        ({  unsigned long _x;                               \
            asm ("mrs %0, " stringify(reg):"=r"(_x));       \
            _x;                                             \
        })

#define set_sysreg(reg, v)                                  \
        ({  unsigned long _x = v;                           \
            asm ("msr " stringify(reg) ", %0"::"r"(_x));    \
            _x;                                             \
        })

#define SCTRL_SPINTMASK     (1UL << 62)
#define SCTRL_NMI           (1UL << 61)
#define SCTRL_EE            (1UL << 25)
#define SCTRL_E0E           (1UL << 24)
#define SCTRL_WXN           (1UL << 19)
#define SCTRL_nAA           (1UL << 6)
#define SCTRL_SA0           (1UL << 4)
#define SCTRL_SA            (1UL << 3)
#define SCTRL_A             (1UL << 1)
#define SCTRL_M             (1UL << 0)

#define sysreg_flagging(reg, set, unset)                    \
        ({                                                  \
            unsigned long _x;                               \
            _x = read_sysreg(reg);                          \
            _x = (_x & ~(unset)) | (set);                   \
            set_sysreg(reg, _x);                            \
            _x;                                             \
        })

#endif
#endif /* __LUNAIX_AA64_MSRS_H */
