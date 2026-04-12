#ifdef CONFIG_ARCH_X86_64
#ifndef __LUNAIX_PT_DEF64_H
#define __LUNAIX_PT_DEF64_H

#include <lunaix/types.h>

#define _PTW_LEVEL              4

#define _PAGE_BASE_SHIFT        12

#define _LFT_LEVEL_WIDTH        9
#define _L0T_LEVEL_WIDTH        _LFT_LEVEL_WIDTH
#define _L1T_LEVEL_WIDTH        _LFT_LEVEL_WIDTH
#define _L2T_LEVEL_WIDTH        _LFT_LEVEL_WIDTH
#define _L3T_LEVEL_WIDTH        0

#define _VA_BITS                48
#define _PA_BITS                52

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
#define _PTE_NX                 (1UL << 63)
#define _PTE_X                  (0)
#define _PTE_R                  (0)

#define __MEMGUARD               0xf0f0f0f0f0f0f0f0UL

typedef unsigned long pte_attr_t;
typedef unsigned long pfn_t;

#endif /* __LUNAIX_PT_DEF64_H */
#endif
