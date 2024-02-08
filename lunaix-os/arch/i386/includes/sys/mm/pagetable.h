#ifndef __LUNAIX_ARCH_PAGETABLE_H
#define __LUNAIX_ARCH_PAGETABLE_H

#include <lunaix/types.h>
#include <lunaix/compiler.h>

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

/*
    Defines page related attributes for different page table
    hierarchies. In Lunaix, we define five arch-agnostic alias
    to those arch-dependent hierarchies:

        * L0T: Level-0 Table, the root page table    
        * L1T: Level-1 Table, indexed by L0P entries 
        * L2T: Level-2 Table, indexed by L1P entries 
        * L3T: Level-3 Table, indexed by L2P entries 
        * LFT: Leaf-Level Table, indexed by L3P entries    
    
    Therefore, "LnTE" can be used to refer "Entry in a Level-n Table"
    In the context of x86 archiecture (regardless x86_32 or x86_64),
    these naming have the following projection:
        
        * L0T: PD           (32-Bit 2-Level paging)
               PML4         (4-Level Paging)
               PML5         (5-Level Paging)

        * L1T: ( N/A )      (32-Bit 2-Level paging)
               PDP          (4-Level Paging)
               PML4         (5-Level Paging)
        
        * L2T: ( N/A )      (32-Bit 2-Level paging)
               PD           (4-Level Paging)
               PDP          (5-Level Paging)

        * L3T: ( N/A )      (32-Bit 2-Level paging)
               ( N/A )      (4-Level Paging)
               PD           (5-Level Paging)

        * LFT: Page Table   (All)

    In addition, we also defines VMS_{MASK|SIZE} to provide
    information about maximium size of addressable virtual 
    memory space (hence VMS). Which is effectively a 
    "Level -1 Page" (i.e., _PAGE_Ln_SIZE(-1))
*/

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


/* ******** PTE Manipulation ******** */

struct __pte {
    unsigned int val;
} align(4);

typedef unsigned int pfn_t;
typedef unsigned int pte_attr_t;

#define _PTE_P  (1 << 0)
#define _PTE_W  (1 << 1)
#define _PTE_U  (1 << 2)
#define _PTE_WT (1 << 3)
#define _PTE_CD (1 << 4)
#define _PTE_A  (1 << 5)
#define _PTE_D  (1 << 6)
#define _PTE_PS (1 << 7)
#define _PTE_G  (1 << 8)
#define _PTE_X  (0)
#define _PTE_R  (0)

#define _PTE_PROT_MASK  ( _PTE_W | _PTE_U | _PTE_X )

#define KERNEL_EXEC     ( _PTE_P | _PTE_X )
#define KERNEL_DATA     ( _PTE_P | _PTE_W  )
#define KERNEL_RDONLY   ( _PTE_P )

#define USER_EXEC       ( _PTE_P | _PTE_X  | _PTE_U )
#define USER_DATA       ( _PTE_P | _PTE_W  | _PTE_U )
#define USER_RDONLY     ( _PTE_P | _PTE_U )

#define SELF_MAP        ( KERNEL_DATA | _PTE_WT | _PTE_CD )


#define __mkpte_from(pte_val) ((pte_t){ .val = (pte_val) })
#define pte_val(pte) (pte.val)

static inline pte_t
mkpte(ptr_t paddr, pte_attr_t prot)
{
    return __mkpte_from((paddr & ~_PTE_PROT_MASK) | (prot & _PTE_PROT_MASK) | _PTE_P);
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

static inline bool
pte_isnull(pte_t pte)
{
    return !!pte.val;
}

static inline pte_t
pte_mkleaf(pte_t pte) 
{
    return __mkpte_from(pte.val | _PTE_PS);
}

static inline pte_t
pte_mkroot(pte_t pte) 
{
    return __mkpte_from(pte.val & ~_PTE_PS);
}

static inline bool
pte_isleaf(pte_t pte) 
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
    return !!(pte.val & _PTE_W);
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
pte_write_entry(pte_t* ptep, pte_t pte)
{
    ptep->val = pte.val;
}




#endif /* __LUNAIX_ARCH_PAGETABLE_H */
