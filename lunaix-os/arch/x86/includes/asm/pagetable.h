#ifndef __LUNAIX_ARCH_PAGETABLE_H
#define __LUNAIX_ARCH_PAGETABLE_H

#include <lunaix/types.h>
#include <lunaix/compiler.h>

/* ******** Page Table Manipulation ******** */

#ifdef CONFIG_ARCH_X86_64

#include "variants/pt_def64.h"

#else

#include "variants/pt_def32.h"

#endif

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


#define _PTE_PPFN_MASK          ( (~PAGE_MASK & PMS_MASK))
#define _PTE_PROT_MASK          ( ~_PTE_PPFN_MASK )

#define KERNEL_PAGE             ( _PTE_P )
#define KERNEL_EXEC             ( KERNEL_PAGE | _PTE_X )
#define KERNEL_DATA             ( KERNEL_PAGE | _PTE_W | _PTE_NX )
#define KERNEL_RDONLY           ( KERNEL_PAGE | _PTE_NX )
#define KERNEL_ROEXEC           ( KERNEL_PAGE | _PTE_X  )
#define KERNEL_PGTAB            ( KERNEL_PAGE | _PTE_W )

#define USER_PAGE               ( _PTE_P | _PTE_U )
#define USER_EXEC               ( USER_PAGE | _PTE_X )
#define USER_DATA               ( USER_PAGE | _PTE_W | _PTE_NX )
#define USER_RDONLY             ( USER_PAGE | _PTE_NX )
#define USER_ROEXEC             ( USER_PAGE | _PTE_X  )
#define USER_PGTAB              ( USER_PAGE | _PTE_W )
#define USER_DEFAULT              USER_PGTAB

#define SELF_MAP                ( KERNEL_PGTAB | _PTE_WT | _PTE_CD )

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
    return __mkpte_from(pte.val & ~_PTE_NX);
}

static inline pte_t
pte_mknonexec(pte_t pte)
{
    return __mkpte_from(pte.val | _PTE_NX);
}

static inline bool
pte_isexec(pte_t pte)
{
    return !(pte.val & _PTE_NX);
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

pte_t
translate_vmr_prot(unsigned int vmr_prot, pte_t pte);

#endif /* __LUNAIX_ARCH_PAGETABLE_H */
