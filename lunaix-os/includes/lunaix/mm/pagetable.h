#ifndef __LUNAIX_PAGETABLE_H
#define __LUNAIX_PAGETABLE_H

/*
    Defines page related attributes for different page table
    hierarchies. In Lunaix, we define five arch-agnostic alias
    to those arch-dependent hierarchies:

        + L0T: Level-0 Table, the root page table    
        + L1T: Level-1 Table, indexed by L0P entries 
        + L2T: Level-2 Table, indexed by L1P entries 
        + L3T: Level-3 Table, indexed by L2P entries 
        + LFT: Leaf-Level Table (Level-4), indexed by L3P entries    
    
    Therefore, "LnTE" can be used to refer "Entry in a Level-n Table".
    Consequently, we can further define 
        
        + LnTEP - pointer to an entry within LnT
        + LnTP  - pointer to (the first entry of) LnT
        
    To better identify all derived value from virtual and physical 
    adress, we defines:
        
        + Virtual Address Space (VAS):
          A set of all virtual addresses that can be interpreted
          by page table walker.

        + Virtual Mappings Space (VMS):
          A imaginary linear table compose a set of mappings that 
          define the translation rules from virtual memory address 
          space to physical memory address. (Note: mappings are stored 
          in the hierarchy of page tables, however it is transparent, 
          just as indexing into a big linear table, thus 'imaginary')
          A VMS is a realisation of a VAS.

        + Virtual Mappings Mounting (VM_MNT or MNT):
          A subregion within current VAS for where the VA/PA mappings
          of another VMS are accessible.

        + Page Frame Number (PFN): 
          Index of a page with it's base size. For most machine, it
          is 4K. Note, this is not limited to physical address, for 
          virtual address, this is the index of a virtual page within
          it's VMS.

        + Virtual Frame Number (VFN):
          Index of a virtual page within it's parent page table. It's
          range is bounded aboved by maximium number of PTEs per page
          table

    In the context of x86 archiecture (regardless x86_32 or x86_64),
    these naming have the following realisations:
        
        + L0T: PD           (32-Bit 2-Level paging)
               PML4         (4-Level Paging)
               PML5         (5-Level Paging)

        + L1T: ( N/A )      (32-Bit 2-Level paging)
               PDP          (4-Level Paging)
               PML4         (5-Level Paging)
        
        + L2T: ( N/A )      (32-Bit 2-Level paging)
               PD           (4-Level Paging)
               PDP          (5-Level Paging)

        + L3T: ( N/A )      (32-Bit 2-Level paging)
               ( N/A )      (4-Level Paging)
               PD           (5-Level Paging)

        + LFT: Page Table   (All)

        + VAS: 
               [0, 2^32)    (32-Bit 2-Level paging)
               [0, 2^48)    (4-Level Paging)
               [0, 2^57)    (5-Level Paging)

        + PFN (Vitrual): 
               [0, 2^22)    (32-Bit 2-Level paging)
               [0, 2^36)    (4-Level Paging)
               [0, 2^45)    (5-Level Paging)
        
        + PFN (Physical):
               [0, 2^32)    (x86_32)
               [0, 2^52)    (x86_64, all paging modes)

        + VFN: [0, 1024)    (32-Bit 2-Level paging)
               [0, 512)     (4-Level Paging)
               [0, 512)     (5-Level Paging)

    In addition, we also defines VMS_{MASK|SIZE} to provide
    information about maximium size of addressable virtual 
    memory space (hence VMS). Which is effectively a 
    "Level -1 Page" (i.e., _PAGE_Ln_SIZE(-1))


*/


#include "asm/mm_defs.h"
struct __pte;
typedef struct __pte pte_t;

#include <asm/mempart.h>
#include <asm/pagetable.h>
#include <asm/cpu.h>
#include "lunaix/types.h"


#define PT_LEVEL                _PTW_LEVEL
#define LnT_ENABLED(n)	        ((n) < PT_LEVEL - 1)

/**
 * Check if the specified translation level is enabled.
 * Applicable to level 1~3 level. L0T and LFT is always enabled.
 */
#define has_ptlevel(n)          ((n) < PT_LEVEL - 1)

#define VA_BITS                 _VA_BITS
#define PA_BITS                 _PA_BITS

#define VA_MASK                 ( ( 1UL << _VA_BITS ) - 1 )
#define PA_MASK                 ( ( 1UL << _PA_BITS ) - 1 )

#define PAGE_SHIFT              _PAGE_BASE_SHIFT
#define PAGE_SIZE               ( 1UL << PAGE_SHIFT )
#define PAGE_MASK               ( ~( PAGE_SIZE - 1 ) )

#define LFT_PAGE_SHIFT          ( _PAGE_BASE_SHIFT )
#define L3T_PAGE_SHIFT          ( _LFT_LEVEL_WIDTH + LFT_PAGE_SHIFT )
#define L2T_PAGE_SHIFT          ( _L3T_LEVEL_WIDTH + L3T_PAGE_SHIFT )
#define L1T_PAGE_SHIFT          ( _L2T_LEVEL_WIDTH + L2T_PAGE_SHIFT )
#define L0T_PAGE_SHIFT          ( _L1T_LEVEL_WIDTH + L1T_PAGE_SHIFT )

#define L0T_SIZE                ( 1UL << L0T_PAGE_SHIFT )
#define L1T_SIZE                ( 1UL << L1T_PAGE_SHIFT )
#define L2T_SIZE                ( 1UL << L2T_PAGE_SHIFT )
#define L3T_SIZE                ( 1UL << L3T_PAGE_SHIFT )
#define LFT_SIZE                ( PAGE_SIZE )

#define L0T_LENGTH              ( 1 << _L0T_LEVEL_WIDTH )
#define L1T_LENGTH              ( 1 << _L1T_LEVEL_WIDTH )
#define L2T_LENGTH              ( 1 << _L2T_LEVEL_WIDTH )
#define L3T_LENGTH              ( 1 << _L3T_LEVEL_WIDTH )
#define LFT_LENGTH              ( 1 << _LFT_LEVEL_WIDTH )


#define L0T_MASK                ( ( 1UL << _L0T_LEVEL_WIDTH ) - 1 )
#define L1T_MASK                ( ( 1UL << _L1T_LEVEL_WIDTH ) - 1 )
#define L2T_MASK                ( ( 1UL << _L2T_LEVEL_WIDTH ) - 1 )
#define L3T_MASK                ( ( 1UL << _L3T_LEVEL_WIDTH ) - 1 )
#define LFT_MASK                ( ( 1UL << _LFT_LEVEL_WIDTH ) - 1 )

#define L0T                     0
#define L1T                     1
#define L2T                     2
#define L3T                     3
#define LFT                     F

#define _level_size(n)          L##n##T_SIZE
#define _level_page_shift(n)    L##n##T_PAGE_SHIFT
#define _level_length(n)        L##n##T_LENGTH
#define _level_mask(n)          L##n##T_MASK
#define _level_width(n)         L##n##T_WIDTH
#define _level_index(va, n)     (((va) >> L##n##T_PAGE_SHIFT) & L##n##T_MASK)

#define level_size(n)           _level_size(n)
#define level_page_shift(n)     _level_page_shift(n)
#define level_length(n)         _level_length(n)
#define level_mask(n)           _level_mask(n)
#define level_width(n)          _level_width(n)
#define level_index(va, n)      _level_index(va, n)

#define __sanitize_vaddr(va)    ( (va) & VA_MASK )
#define __sanitize_paddr(va)    ( (va) & PA_MASK )
#define __vaddr_tag(va)         ( (va) >> _VA_BITS )

#define ptep_at(table, va, level)       \
    ( &((pte_t*)(table))[_level_index(va, level)] )

static inline pfn_t
page_index(ptr_t addr_like)
{
    return addr_like / PAGE_SIZE;
}

static inline unsigned int
page_offset(ptr_t addr_like)
{
    return addr_like & ~PAGE_MASK;
}

static inline ptr_t
page_frame(ptr_t addr_like)
{
    return addr_like & PAGE_MASK;
}

static inline int
ptep_entry_index(pte_t* ptep)
{
    return page_offset((ptr_t)ptep) / sizeof(pte_t);
}

static inline void
set_ptes_level(pte_t* ptep, pte_t attrs, ptr_t pa, int n, size_t lsize)
{
    while (n--) {
        set_pte(ptep, pte_setpaddr(attrs, pa));

        ptep++;
        pa += lsize;
    }
}

static inline void
set_ptes(pte_t* ptep, pte_t attrs, ptr_t pa, int n)
{
    set_ptes_level(ptep, attrs, pa, n, PAGE_SIZE);
}

static inline void
fill_ptes(pte_t* ptep, pte_t val, int nr)
{
    while (nr--)
        set_pte(ptep++, val);
}

static inline pte_t*
ptep_next_table(pte_t* ptep)
{
    pte_t pte;

    pte = pte_at(ptep);
    if (pte_isnull(pte) || pte_huge(pte))
        return NULL;

    return (pte_t*)phy_to_virt(pte_paddr(pte));
}

static inline ptr_t
pte_page_va(pte_t pte)
{
    return phy_to_virt(pte_paddr(pte));
}

#endif /* __LUNAIX_PAGETABLE_H */
