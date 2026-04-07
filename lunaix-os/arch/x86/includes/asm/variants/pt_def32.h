#ifdef CONFIG_ARCH_I386

#ifndef __LUNAIX_PT_DEF32_H
#define __LUNAIX_PT_DEF32_H

#define _PTW_LEVEL              2

#define _PAGE_BASE_SHIFT        12

#define _LFT_LEVEL_WIDTH        10
#define _L0T_LEVEL_WIDTH        _LFT_LEVEL_WIDTH
#define _L1T_LEVEL_WIDTH        0 
#define _L2T_LEVEL_WIDTH        0 
#define _L3T_LEVEL_WIDTH        0

#define _VA_BITS                32
#define _PA_BITS                32

struct __pte {
    unsigned int val;
} align(4);

#define _PTE_P                  (1 << 0)
#define _PTE_W                  (1 << 1)
#define _PTE_U                  (1 << 2)
#define _PTE_WT                 (1 << 3)
#define _PTE_CD                 (1 << 4)
#define _PTE_A                  (1 << 5)
#define _PTE_D                  (1 << 6)
#define _PTE_PS                 (1 << 7)
#define _PTE_PAT                (1 << 7)
#define _PTE_G                  (1 << 8)
#define _PTE_X                  (0)
#define _PTE_NX                 (0)
#define _PTE_R                  (0)

#define __MEMGUARD               0xdeadc0deUL

typedef unsigned int pte_attr_t;
typedef unsigned int pfn_t;

#endif /* __LUNAIX_PT_DEF32_H */
#endif
