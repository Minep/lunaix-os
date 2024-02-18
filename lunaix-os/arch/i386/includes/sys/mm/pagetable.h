#ifndef __LUNAIX_ARCH_PAGETABLE_H
#define __LUNAIX_ARCH_PAGETABLE_H

#include <lunaix/types.h>
#include <lunaix/compiler.h>
#include <sys/cpu.h>

/* ******** Page Table Manipulation ******** */

// Levels of page table to traverse for a single page walk
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

#define VMS_MASK            ( -1UL )
#define VMS_SIZE            VMS_MASK

/* General size of a LnT huge page */

#define L0T_SIZE            _PAGE_Ln_SIZE(0)
#define L1T_SIZE            _PAGE_Ln_SIZE(1)
#define L2T_SIZE            _PAGE_Ln_SIZE(1)
#define L3T_SIZE            _PAGE_Ln_SIZE(1)
#define LFT_SIZE            _PAGE_Ln_SIZE(1)

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

#define PAGE_SIZE           _PAGE_BASE_SIZE
#define PAGE_MASK           _PAGE_BASE_MASK
#define LEVEL_SIZE          _PAGE_LEVEL_SIZE
#define LEVEL_MASK          _PAGE_LEVEL_MASK

// max PTEs number
#define MAX_PTEN            _PAGE_LEVEL_SIZE

// max translation level supported
#define MAX_LEVEL           _PTW_LEVEL


/* ******** PTE Manipulation ******** */

struct __pte {
    unsigned int val;
} align(4);

typedef unsigned int pfn_t;
typedef unsigned int pte_attr_t;

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
#define _PTE_R                  (0)

#define _PTE_PROT_MASK          ( _PTE_W | _PTE_U | _PTE_X )

#define KERNEL_PAGE             ( _PTE_P )
#define KERNEL_EXEC             ( KERNEL_PAGE | _PTE_X )
#define KERNEL_DATA             ( KERNEL_PAGE | _PTE_W  )
#define KERNEL_RDONLY           ( KERNEL_PAGE )

#define USER_PAGE               ( _PTE_P | _PTE_U )
#define USER_EXEC               ( USER_PAGE | _PTE_X )
#define USER_DATA               ( USER_PAGE | _PTE_W )
#define USER_RDONLY             ( USER_PAGE )

#define SELF_MAP                ( KERNEL_DATA | _PTE_WT | _PTE_CD )

#define __mkpte_from(pte_val)   ((pte_t){ .val = (pte_val) })
#define __MEMGUARD               0xdeadc0deUL

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
    pte_attr_t attrs = (prot & _PTE_PROT_MASK) | _PTE_P;
    return __mkpte_from(attrs);
}

static inline pte_t
mkpte(ptr_t paddr, pte_attr_t prot)
{
    pte_attr_t attrs = (prot & _PTE_PROT_MASK) | _PTE_P;
    return __mkpte_from((paddr & ~_PAGE_BASE_MASK) | attrs);
}

static inline pte_t
mkpte_root(ptr_t paddr, pte_attr_t prot)
{
    pte_attr_t attrs = (prot & _PTE_PROT_MASK) | _PTE_P;
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
    return __mkpte_from((pte.val & _PAGE_BASE_MASK) | (paddr & ~_PAGE_BASE_MASK));
}

static inline ptr_t
pte_paddr(pte_t pte)
{
    return pte.val & ~_PAGE_BASE_MASK;
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
    return __mkpte_from(pte.val | _PTE_PS);
}

static inline pte_t
pte_mkvolatile(pte_t pte) 
{
    return __mkpte_from(pte.val | _PTE_WT | _PTE_CD);
}

static inline pte_t
pte_mkroot(pte_t pte) 
{
    return __mkpte_from(pte.val & ~_PTE_PS);
}

static inline pte_t
pte_usepat(pte_t pte) 
{
    return __mkpte_from(pte.val | _PTE_PAT);
}

static inline bool
pte_huge(pte_t pte) 
{
    return !!(pte.val & _PTE_PS);
}

static inline pte_t
pte_mkloaded(pte_t pte) 
{
    return __mkpte_from(pte.val | _PTE_P);
}

static inline pte_t
pte_mkunloaded(pte_t pte) 
{
    return __mkpte_from(pte.val & ~_PTE_P);
}

static inline bool
pte_isloaded(pte_t pte) 
{
    return !!(pte.val & _PTE_P);
}

static inline pte_t
pte_mkwprotect(pte_t pte) 
{
    return __mkpte_from(pte.val & ~_PTE_W);
}

static inline pte_t
pte_mkwritable(pte_t pte) 
{
    return __mkpte_from(pte.val | _PTE_W);
}

static inline bool
pte_iswprotect(pte_t pte) 
{
    return !(pte.val & _PTE_W);
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
    return __mkpte_from(pte.val | _PTE_X);
}

static inline pte_t
pte_mknonexec(pte_t pte)
{
    return __mkpte_from(pte.val & ~_PTE_X);
}

static inline bool
pte_isexec(pte_t pte)
{
    return !!(pte.val & _PTE_X);
}

static inline pte_t
pte_mkuntouch(pte_t pte) 
{
    return __mkpte_from(pte.val & ~_PTE_A);
}

static inline bool
pte_istouched(pte_t pte) 
{
    return !!(pte.val & _PTE_A);
}

static inline pte_t
pte_mkclean(pte_t pte) 
{
    return __mkpte_from(pte.val & ~_PTE_D);
}

static inline bool
pte_dirty(pte_t pte) 
{
    return !!(pte.val & _PTE_D);
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


#endif /* __LUNAIX_ARCH_PAGETABLE_H */
