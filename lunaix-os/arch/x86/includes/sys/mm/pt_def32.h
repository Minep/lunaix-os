#ifdef CONFIG_ARCH_I386

#ifndef __LUNAIX_PT_DEF32_H
#define __LUNAIX_PT_DEF32_H

#define _PTW_LEVEL          2

#define _PAGE_BASE_SHIFT    12
#define _PAGE_BASE_SIZE     ( 1UL << _PAGE_BASE_SHIFT )
#define _PAGE_BASE_MASK     ( _PAGE_BASE_SIZE - 1)

#define _PAGE_LEVEL_SHIFT   10
#define _PAGE_LEVEL_SIZE    ( 1UL << _PAGE_LEVEL_SHIFT )
#define _PAGE_LEVEL_MASK    ( _PAGE_LEVEL_SIZE - 1 )
#define _PAGE_Ln_SIZE(n)    ( 1UL << (_PAGE_BASE_SHIFT + _PAGE_LEVEL_SHIFT * (_PTW_LEVEL - (n) - 1)) )

// Note: we set VMS_SIZE = VMS_MASK as it is impossible
//       to express 4Gi in 32bit unsigned integer

#define VMS_BITS            32
#define PMS_BITS            32

#define VMS_SIZE            ( 1UL << VMS_BITS)
#define VMS_MASK            ( VMS_SIZE - 1 )
#define PMS_SIZE            ( 1UL << PMS_BITS )
#define PMS_MASK            ( PMS_SIZE - 1 )

/* General size of a LnT huge page */

#define L0T_SIZE            _PAGE_Ln_SIZE(0)
#define L1T_SIZE            _PAGE_Ln_SIZE(1)
#define L2T_SIZE            _PAGE_Ln_SIZE(1)
#define L3T_SIZE            _PAGE_Ln_SIZE(1)
#define LFT_SIZE            _PAGE_Ln_SIZE(1)

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

#define __index(va)             ( (va) & VMS_MASK )
#define __vaddr(va)             (va)
#define __paddr(pa)             ( (pa) & PMS_MASK )

#endif /* __LUNAIX_PT_DEF32_H */
#endif
