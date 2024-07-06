#ifdef CONFIG_ARCH_X86_64
#ifndef __LUNAIX_PT_DEF64_H
#define __LUNAIX_PT_DEF64_H

#include <lunaix/types.h>

#define _PTW_LEVEL          4


// Note: we set VMS_SIZE = VMS_MASK as it is impossible
//       to express 4Gi in 32bit unsigned integer

#define _PAGE_BASE_SHIFT    12

#define _PAGE_BASE_SIZE     ( 1UL << _PAGE_BASE_SHIFT )
#define _PAGE_BASE_MASK     ( (_PAGE_BASE_SIZE - 1) & 0x000fffffffffffffUL )

#define _PAGE_LEVEL_SHIFT   9
#define _PAGE_LEVEL_SIZE    ( 1UL << _PAGE_LEVEL_SHIFT )
#define _PAGE_LEVEL_MASK    ( _PAGE_LEVEL_SIZE - 1 )
#define _PAGE_Ln_SIZE(n)    ( 1UL << (_PAGE_BASE_SHIFT + _PAGE_LEVEL_SHIFT * (_PTW_LEVEL - (n) - 1)) )

#define VMS_MASK            ( 0x0000ffffffffffffUL )
#define VMS_SIZE            (VMS_MASK + 1)

/* General size of a LnT huge page */

#define L0T_SIZE            _PAGE_Ln_SIZE(0)
#define L1T_SIZE            _PAGE_Ln_SIZE(1)
#define L2T_SIZE            _PAGE_Ln_SIZE(2)
#define L3T_SIZE            _PAGE_Ln_SIZE(3)
#define LFT_SIZE            _PAGE_Ln_SIZE(3)


struct __pte {
    unsigned long val;
} align(8);

#define _PTE_P                  (1UL << 0)
#define _PTE_W                  (1UL << 1)
#define _PTE_U                  (1UL << 2)
#define _PTE_WT                 (1UL << 3)
#define _PTE_CD                 (1UL << 4)
#define _PTE_A                  (1UL << 5)
#define _PTE_D                  (1UL << 6)
#define _PTE_PS                 (1UL << 7)
#define _PTE_PAT                (1UL << 7)
#define _PTE_G                  (1UL << 8)
#define _PTE_X                  (1UL << 63)
#define _PTE_R                  (0)

#define __MEMGUARD               0xf0f0f0f0f0f0f0f0UL

#endif /* __LUNAIX_PT_DEF64_H */
#endif