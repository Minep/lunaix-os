#ifndef __LUNAIX_ARCH_PAGETABLE_H
#define __LUNAIX_ARCH_PAGETABLE_H

#include <lunaix/types.h>
#include <lunaix/compiler.h>

#include "aa64_mmu.h"

/* ******** Page Table Manipulation ******** */

#define _PTW_LEVEL          4


// Note: we set VMS_SIZE = VMS_MASK as it is impossible
//       to express 4Gi in 32bit unsigned integer


#define VMS_BITS            48

#define PMS_BITS            CONFIG_AA64_OA_SIZE

#define VMS_SIZE            ( 1UL << VMS_BITS)
#define VMS_MASK            ( VMS_SIZE - 1 )
#define PMS_SIZE            ( 1UL << PMS_BITS )
#define PMS_MASK            ( PMS_SIZE - 1 )

#define __index(va)             ( (va) & VMS_MASK )
#define __vaddr(va)             \
        ( (__index(va) ^ ((VMS_MASK + 1) >> 1)) - ((VMS_MASK + 1) >> 1) )
#define __paddr(pa)             ( (pa) & PMS_MASK )


#if    defined(CONFIG_AA64_PAGE_GRAN_4K)
#define _PAGE_BASE_SHIFT    12
#elif  defined(CONFIG_AA64_PAGE_GRAN_16K)
#define _PAGE_BASE_SHIFT    14
#elif  defined(CONFIG_AA64_PAGE_GRAN_64K)
#define _PAGE_BASE_SHIFT    16
#endif

#define _PAGE_BASE_SIZE     ( 1UL << _PAGE_BASE_SHIFT )
#define _PAGE_BASE_MASK     ( (_PAGE_BASE_SIZE - 1) & VMS_MASK )

#define _PAGE_LEVEL_SHIFT   9
#define _PAGE_LEVEL_SIZE    ( 1UL << _PAGE_LEVEL_SHIFT )
#define _PAGE_LEVEL_MASK    ( _PAGE_LEVEL_SIZE - 1 )
#define _PAGE_Ln_SIZE(n)    \
    ( 1UL << (_PAGE_BASE_SHIFT + _PAGE_LEVEL_SHIFT * (_PTW_LEVEL - (n) - 1)) )

/* General size of a LnT huge page */

#define L0T_SIZE            _PAGE_Ln_SIZE(0)
#define L1T_SIZE            _PAGE_Ln_SIZE(1)
#define L2T_SIZE            _PAGE_Ln_SIZE(2)
#define L3T_SIZE            _PAGE_Ln_SIZE(3)
#define LFT_SIZE            _PAGE_Ln_SIZE(3)


struct __pte {
    unsigned long val;
} align(8);

// upper attributes

#define _PTE_UXN                (1UL << 54)
#define _PTE_PXN                (1UL << 53)
#define _PTE_XN                 (_PTE_UXN | _PTE_PXN)
#define _PTE_Contig             (1UL << 52)
#define _PTE_DBM                (1UL << 51)

#ifdef _MMU_USE_OA52
#if  CONFIG_AA64_PAGE_GRAN_64K
#define __OA_HIGH_MASK          ( 0b1111 << 12 )
#define __OA_HEAD(pa)           ((pa) & ((1UL << 48) - 1) & ~PAGE_MASK)
#define __OA_TAIL(pa)           ((((pa) >> 48) & 0b1111) << 12)
#else
#define __OA_HIGH_MASK          ( 0b0011 << 8 )
#define __OA_HEAD(pa)           ((pa) & ((1UL << 50) - 1) & ~PAGE_MASK)
#define __OA_TAIL(pa)           ((((pa) >> 50) & 0b0011) << 8)
#endif
#else
#define __OA_HIGH_MASK          (0)
#define __OA_HEAD(pa)           (__paddr(pa) & ~PAGE_MASK)
#define __OA_TAIL(pa)           (0)
#endif

#define _PTE_OA(pa)             (__OA_HEAD(pa) | __OA_TAIL(pa))

// lower attributes

#define _PTE_nG                 (1UL << 11)
#define _PTE_AF                 (1UL << 10)

// AP bits: R_RNGJG

#define _PTE_AP(p, u)           ((((p) & 1) << 1 | ((u) & 1)) << 6)
#define _PTE_PRW                _PTE_AP(0 , 0)      // priv rw, unpriv none
#define _PTE_PRWURW             _PTE_AP(0 , 1)      // priv rw, unpriv rw
#define _PTE_U                  _PTE_AP(0 , 1)      // generic unpriv flag
#define _PTE_PRO                _PTE_AP(1 , 0)      // priv ro, unpriv none
#define _PTE_PROURO             _PTE_AP(1 , 1)      // priv ro, unpriv ro

#define _PTE_BLKDESC            (0b01)
#define _PTE_TABDESC            (0b11)
#define _PTE_LFTDESC            (0b11)
#define _PTE_VALID              (0b01)
#define _PTE_DESC_MASK          (0b11)
#define _PTE_SET_DESC(pte_val, desc)    \
        ( ((pte_val) & ~_PTE_DESC_MASK) | ((desc) & _PTE_DESC_MASK) )
#define _PTE_GET_DESC(pte_val)          \
        ( (pte_val) & _PTE_DESC_MASK )

#define __MEMGUARD               0xf0f0f0f0f0f0f0f0UL

typedef unsigned long pte_attr_t;
typedef unsigned long pfn_t;

// always do sign extend on x86_64



/* General mask to get page offset of a LnT huge page */

#define L0T_MASK            ( L0T_SIZE - 1 )
#define L1T_MASK            ( L1T_SIZE - 1 )
#define L2T_MASK            ( L2T_SIZE - 1 )
#define L3T_MASK            ( L3T_SIZE - 1 )
#define LFT_MASK            ( LFT_SIZE - 1 )

/* Masks to get index of a LnTE */

#define L0T_INDEX_MASK      ( VMS_MASK ^ L0T_MASK )
#define L1T_INDEX_MASK      ( L0T_MASK ^ L1T_MASK )
#define L2T_INDEX_MASK      ( L1T_MASK ^ L2T_MASK )
#define L3T_INDEX_MASK      ( L2T_MASK ^ L3T_MASK )
#define LFT_INDEX_MASK      ( L3T_MASK ^ LFT_MASK )

#define PAGE_SHIFT          _PAGE_BASE_SHIFT
#define PAGE_SIZE           _PAGE_BASE_SIZE
#define PAGE_MASK           _PAGE_BASE_MASK

#define LEVEL_SHIFT         _PAGE_LEVEL_SHIFT
#define LEVEL_SIZE          _PAGE_LEVEL_SIZE
#define LEVEL_MASK          _PAGE_LEVEL_MASK

// max PTEs number
#define MAX_PTEN            _PAGE_LEVEL_SIZE

// max translation level supported
#define MAX_LEVEL           _PTW_LEVEL

typedef struct __pte pte_t;

#define _PTE_PROT_MASK          ( ~((1UL << 50) - 1) | (PAGE_MASK & __OA_HIGH_MASK) )
#define _PTE_PPFN_MASK          ( ~_PTE_PROT_MASK )

#define _PAGE_BASIC             ( _PTE_VALID )

#define KERNEL_EXEC             ( _PAGE_BASIC | _PTE_PRO | _PTE_UXN )
#define KERNEL_DATA             ( _PAGE_BASIC | _PTE_PRW | _PTE_XN )
#define KERNEL_RDONLY           ( _PAGE_BASIC | _PTE_PRO | _PTE_XN )
#define KERNEL_ROEXEC           KERNEL_EXEC
#define KERNEL_PGTAB            ( _PAGE_BASIC | _PTE_TABDESC )

#define USER_EXEC               ( _PAGE_BASIC | _PTE_PROURO | _PTE_PXN )
#define USER_DATA               ( _PAGE_BASIC | _PTE_PRWURW | _PTE_XN )
#define USER_RDONLY             ( _PAGE_BASIC | _PTE_PROURO )
#define USER_ROEXEC             USER_EXEC
#define USER_PGTAB              ( _PAGE_BASIC | _PTE_TABDESC )

#define SELF_MAP                ( KERNEL_DATA | _PTE_TABDESC )

#define __mkpte_from(pte_val)   ((pte_t){ .val = (pte_val) })

#define null_pte                ( __mkpte_from(0) )
#define guard_pte               ( __mkpte_from(__MEMGUARD) )
#define pte_val(pte)            ( pte.val )


static inline bool
pte_isguardian(pte_t pte)
{
    return pte.val == __MEMGUARD;
}

static inline pte_t
mkpte_prot(pte_attr_t prot)
{
    pte_attr_t attrs = (prot & _PTE_PROT_MASK) | _PTE_LFTDESC;
    return __mkpte_from(attrs);
}

static inline pte_t
mkpte(ptr_t paddr, pte_attr_t prot)
{
    pte_attr_t attrs = (prot & _PTE_PROT_MASK) | _PTE_LFTDESC;
    return __mkpte_from((paddr & ~_PAGE_BASE_MASK) | attrs);
}

static inline pte_t
mkpte_root(ptr_t paddr, pte_attr_t prot)
{
    pte_attr_t attrs = (prot & _PTE_PROT_MASK) | _PTE_TABDESC;
    return __mkpte_from((paddr & ~_PAGE_BASE_MASK) | attrs);
}

static inline pte_t
mkpte_raw(unsigned long pte_val)
{
    return __mkpte_from(pte_val);
}

static inline pte_t
pte_setpaddr(pte_t pte, ptr_t paddr)
{
    return __mkpte_from((pte.val & _PTE_PROT_MASK) | (paddr & ~_PTE_PROT_MASK));
}

static inline pte_t
pte_setppfn(pte_t pte, pfn_t ppfn)
{
    return pte_setpaddr(pte, ppfn * PAGE_SIZE);
}

static inline ptr_t
pte_paddr(pte_t pte)
{
    return __paddr(pte.val) & ~_PTE_PROT_MASK;
}

static inline pfn_t
pte_ppfn(pte_t pte)
{
    return pte_paddr(pte) >> _PAGE_BASE_SHIFT;
}

static inline pte_t
pte_setprot(pte_t pte, ptr_t prot)
{
    return __mkpte_from((pte.val & ~_PTE_PROT_MASK) | (prot & _PTE_PROT_MASK));
}

static inline pte_attr_t
pte_prot(pte_t pte)
{
    return (pte.val & _PTE_PROT_MASK);
}

static inline bool
pte_isnull(pte_t pte)
{
    return !pte.val;
}

static inline pte_t
pte_mkhuge(pte_t pte) 
{
    return __mkpte_from(_PTE_SET_DESC(pte.val, _PTE_BLKDESC));
}

static inline pte_t
pte_mkvolatile(pte_t pte) 
{
    return __mkpte_from(pte.val);
}

static inline pte_t
pte_mkroot(pte_t pte) 
{
    return __mkpte_from(_PTE_SET_DESC(pte.val, _PTE_TABDESC));
}

static inline bool
pte_huge(pte_t pte) 
{
    return _PTE_GET_DESC(pte.val) == _PTE_BLKDESC;
}

static inline pte_t
pte_mkloaded(pte_t pte) 
{
    return __mkpte_from(pte.val | _PTE_VALID);
}

static inline pte_t
pte_mkunloaded(pte_t pte) 
{
    return __mkpte_from(pte.val & ~_PTE_VALID);
}

static inline bool
pte_isloaded(pte_t pte) 
{
    return !!(pte.val & _PTE_VALID);
}

static inline pte_t
pte_mkwprotect(pte_t pte) 
{
    return __mkpte_from(pte.val | _PTE_PRO);
}

static inline pte_t
pte_mkwritable(pte_t pte) 
{
    return __mkpte_from(pte.val & ~_PTE_PRO);
}

static inline bool
pte_iswprotect(pte_t pte) 
{
    return !(pte.val & _PTE_PRO);
}

static inline pte_t
pte_mkuser(pte_t pte) 
{
    return __mkpte_from(pte.val | _PTE_U);
}

static inline pte_t
pte_mkkernel(pte_t pte) 
{
    return __mkpte_from(pte.val & ~_PTE_U);
}

static inline bool
pte_allow_user(pte_t pte) 
{
    return !!(pte.val & _PTE_U);
}

static inline pte_t
pte_mkexec(pte_t pte)
{
    return __mkpte_from(pte.val & ~_PTE_PXN);
}

static inline pte_t
pte_mknexec(pte_t pte)
{
    return __mkpte_from(pte.val | _PTE_PXN);
}

static inline pte_t
pte_mkuexec(pte_t pte)
{
    return __mkpte_from(pte.val & ~_PTE_UXN);
}

static inline pte_t
pte_mknuexec(pte_t pte)
{
    return __mkpte_from(pte.val | _PTE_UXN);
}

static inline bool
pte_isexec(pte_t pte)
{
    return !(pte.val & _PTE_PXN);
}

static inline bool
pte_isuexec(pte_t pte)
{
    return !(pte.val & _PTE_UXN);
}

static inline pte_t
pte_mkuntouch(pte_t pte) 
{
    return __mkpte_from(pte.val & ~_PTE_AF);
}

static inline bool
pte_istouched(pte_t pte) 
{
    return !!(pte.val & _PTE_AF);
}

static inline pte_t
pte_mkclean(pte_t pte) 
{
    return __mkpte_from(pte.val & ~_PTE_DBM);
}

static inline bool
pte_dirty(pte_t pte) 
{
    return !!(pte.val & _PTE_DBM);
}

static inline void
set_pte(pte_t* ptep, pte_t pte)
{
    ptep->val = pte.val;
}

static inline pte_t
pte_at(pte_t* ptep) {
    return *ptep;
}

pte_t
translate_vmr_prot(unsigned int vmr_prot, pte_t pte);

#endif /* __LUNAIX_ARCH_PAGETABLE_H */
