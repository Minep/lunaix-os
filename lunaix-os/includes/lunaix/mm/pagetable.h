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

struct __pte;
typedef struct __pte pte_t;


#include <asm/mempart.h>
#include <asm/pagetable.h>
#include <asm/cpu.h>

#define VMS_SELF                VMS_SELF_MOUNT
#define VMS_SELF_L0TI           (__index(VMS_SELF_MOUNT) / L0T_SIZE)

#define _LnT_LEVEL_SIZE(n)      ( L##n##T_SIZE / PAGE_SIZE )
#define _LFTEP_SELF             ( __index(VMS_SELF) )
#define _L3TEP_SELF             ( _LFTEP_SELF | (_LFTEP_SELF / _LnT_LEVEL_SIZE(3)) )
#define _L2TEP_SELF             ( _L3TEP_SELF | (_LFTEP_SELF / _LnT_LEVEL_SIZE(2)) )
#define _L1TEP_SELF             ( _L2TEP_SELF | (_LFTEP_SELF / _LnT_LEVEL_SIZE(1)) )
#define _L0TEP_SELF             ( _L1TEP_SELF | (_LFTEP_SELF / _LnT_LEVEL_SIZE(0)) )

#define _L0TEP_AT(vm_mnt)       ( ((vm_mnt) | (_L0TEP_SELF & L0T_MASK)) )
#define _L1TEP_AT(vm_mnt)       ( ((vm_mnt) | (_L1TEP_SELF & L0T_MASK)) )
#define _L2TEP_AT(vm_mnt)       ( ((vm_mnt) | (_L2TEP_SELF & L0T_MASK)) )
#define _L3TEP_AT(vm_mnt)       ( ((vm_mnt) | (_L3TEP_SELF & L0T_MASK)) )
#define _LFTEP_AT(vm_mnt)       ( ((vm_mnt) | (_LFTEP_SELF & L0T_MASK)) )

#define _VM_OF(ptep)            ( (ptr_t)(ptep) & ~L0T_MASK )
#define _VM_PFN_OF(ptep)        ( ((ptr_t)(ptep) & L0T_MASK) / sizeof(pte_t) )

#define __LnTI_OF(ptep, n)\
    ( __index(_VM_PFN_OF(ptep) * LFT_SIZE / L##n##T_SIZE) )

#define __LnTEP(ptep, va, n)\
    ( (pte_t*)_L##n##TEP_AT(_VM_OF(ptep)) + (__index(va) / L##n##T_SIZE) )

#define __LnTEP_OF(ptep, n)\
    ( (pte_t*)_L##n##TEP_AT(_VM_OF(ptep)) + __LnTI_OF(ptep, n))

#define __LnTEP_SHIFT_NEXT(ptep)\
    ( (pte_t*)(_VM_OF(ptep) | ((_VM_PFN_OF(ptep) * LFT_SIZE) & L0T_MASK)) )

#define _has_LnT(n) (L##n##T_SIZE != LFT_SIZE)
#define LnT_ENABLED(n) _has_LnT(n)

extern pte_t 
alloc_kpage_at(pte_t* ptep, pte_t pte, int order);

/**
 * @brief Try page walk to the pte pointed by ptep and 
 *        allocate any missing level-table en-route
 * 
 * @param ptep 
 * @param va 
 */
void
ptep_alloc_hierarchy(pte_t* ptep, ptr_t va, pte_attr_t prot);

static inline bool
__alloc_level(pte_t* ptep, pte_t pte, pte_attr_t prot)
{
    if (!pte_isnull(pte)) {
        return true;
    }

    pte = pte_setprot(pte, prot);
    return !pte_isnull(alloc_kpage_at(ptep, pte, 0));
}

/**
 * @brief Get the page frame number encoded in ptep
 * 
 * @param ptep 
 * @return ptr_t 
 */
static inline ptr_t
ptep_pfn(pte_t* ptep)
{
    return _VM_PFN_OF(ptep);
}

/**
 * @brief Get the virtual frame number encoded in ptep
 * 
 * @param ptep 
 * @return ptr_t 
 */
static inline unsigned int
ptep_vfn(pte_t* ptep)
{
    return ((ptr_t)ptep & PAGE_MASK) / sizeof(pte_t);
}

static inline ptr_t
ptep_va(pte_t* ptep, size_t lvl_size)
{
    return __vaddr(ptep_pfn(ptep) * lvl_size);
}

static inline ptr_t
ptep_vm_mnt(pte_t* ptep)
{
    return __vaddr(_VM_OF(ptep));
}

/**
 * @brief Make a L0TEP from given ptep
 * 
 * @param ptep 
 * @return pte_t* 
 */
static inline pte_t*
mkl0tep(pte_t* ptep)
{
    return __LnTEP_OF(ptep, 0);
}

/**
 * @brief Make a L1TEP from given ptep
 * 
 * @param ptep 
 * @return pte_t* 
 */
static inline pte_t*
mkl1tep(pte_t* ptep)
{
    return __LnTEP_OF(ptep, 1);
}

/**
 * @brief Make a L2TEP from given ptep
 * 
 * @param ptep 
 * @return pte_t* 
 */
static inline pte_t*
mkl2tep(pte_t* ptep)
{
    return __LnTEP_OF(ptep, 2);
}

/**
 * @brief Make a L3TEP from given ptep
 * 
 * @param ptep 
 * @return pte_t* 
 */
static inline pte_t*
mkl3tep(pte_t* ptep)
{
    return __LnTEP_OF(ptep, 3);
}

/**
 * @brief Create the L1T pointed by L0TE
 * 
 * @param l0t_ptep 
 * @param va 
 * @return pte_t* 
 */
static inline pte_t*
mkl1t(pte_t* l0t_ptep, ptr_t va, pte_attr_t prot)
{
#if _has_LnT(1)
    if (!l0t_ptep) {
        return NULL;
    }

    pte_t pte = pte_at(l0t_ptep);
    
    if (pte_huge(pte)) {
        return l0t_ptep;
    }
    
    return __alloc_level(l0t_ptep, pte, prot) 
                ? __LnTEP(l0t_ptep, va, 1) 
                : NULL;
#else
    return l0t_ptep;
#endif
}

/**
 * @brief Create the L2T pointed by L1TE
 * 
 * @param l0t_ptep 
 * @param va 
 * @return pte_t* 
 */
static inline pte_t*
mkl2t(pte_t* l1t_ptep, ptr_t va, pte_attr_t prot)
{
#if _has_LnT(2)
    if (!l1t_ptep) {
        return NULL;
    }

    pte_t pte = pte_at(l1t_ptep);
    
    if (pte_huge(pte)) {
        return l1t_ptep;
    }

    return __alloc_level(l1t_ptep, pte, prot) 
                ? __LnTEP(l1t_ptep, va, 2) 
                : NULL;
#else
    return l1t_ptep;
#endif
}

/**
 * @brief Create the L3T pointed by L2TE
 * 
 * @param l0t_ptep 
 * @param va 
 * @return pte_t* 
 */
static inline pte_t*
mkl3t(pte_t* l2t_ptep, ptr_t va, pte_attr_t prot)
{
#if _has_LnT(3)
    if (!l2t_ptep) {
        return NULL;
    }
 
    pte_t pte = pte_at(l2t_ptep);
    
    if (pte_huge(pte)) {
        return l2t_ptep;
    }

    return __alloc_level(l2t_ptep, pte, prot) 
                ? __LnTEP(l2t_ptep, va, 3) 
                : NULL;
#else
    return l2t_ptep;
#endif
}

/**
 * @brief Create the LFT pointed by L3TE
 * 
 * @param l0t_ptep 
 * @param va 
 * @return pte_t* 
 */
static inline pte_t*
mklft(pte_t* l3t_ptep, ptr_t va, pte_attr_t prot)
{
    if (!l3t_ptep) {
        return NULL;
    }

    pte_t pte = pte_at(l3t_ptep);
    
    if (pte_huge(pte)) {
        return l3t_ptep;
    }

    return __alloc_level(l3t_ptep, pte, prot) 
                ? __LnTEP(l3t_ptep, va, F) 
                : NULL;
}

static inline pte_t*
getl1tep(pte_t* l0t_ptep, ptr_t va) {
#if _has_LnT(1)
    return __LnTEP(l0t_ptep, va, 1); 
#else
    return l0t_ptep;
#endif
}

static inline pte_t*
getl2tep(pte_t* l1t_ptep, ptr_t va) {
#if _has_LnT(2)
    return __LnTEP(l1t_ptep, va, 2);
#else
    return l1t_ptep;
#endif
}

static inline pte_t*
getl3tep(pte_t* l2t_ptep, ptr_t va) {
#if _has_LnT(3)
    return __LnTEP(l2t_ptep, va, 3); 
#else
    return l2t_ptep;
#endif
}

static inline pte_t*
getlftep(pte_t* l3t_ptep, ptr_t va) {
    return __LnTEP(l3t_ptep, va, F);
}

static inline unsigned int
l0te_index(pte_t* ptep) {
    return __LnTI_OF(ptep, 1);
}

static inline unsigned int
l1te_index(pte_t* ptep) {
    return __LnTI_OF(ptep, 2);
}

static inline unsigned int
l2te_index(pte_t* ptep) {
    return __LnTI_OF(ptep, 3);
}

static inline unsigned int
l3te_index(pte_t* ptep) {
    return __LnTI_OF(ptep, F);
}

static inline pfn_t
pfn(ptr_t addr) {
    return __index(addr) / PAGE_SIZE;
}

static inline size_t
leaf_count(size_t size) {
    return (size + PAGE_MASK) / PAGE_SIZE;
}

static inline size_t
page_count(size_t size, size_t page_size) {
    return (size + (page_size - 1)) / page_size;
}

static inline unsigned int
va_offset(ptr_t addr) {
    return addr & PAGE_MASK;
}

static inline ptr_t
page_addr(ptr_t pfn) {
    return __vaddr(pfn * PAGE_SIZE);
}

static inline ptr_t
page_aligned(ptr_t va) {
    return va & ~PAGE_MASK;
}

static inline ptr_t
page_upaligned(ptr_t va) {
    return (va + PAGE_MASK) & ~PAGE_MASK;
}

static inline ptr_t
napot_aligned(ptr_t va, size_t napot_sz) {
    return va & ~(napot_sz - 1);
}

static inline ptr_t
napot_upaligned(ptr_t va, size_t napot_sz) {
    return (va + napot_sz - 1) & ~(napot_sz - 1);
}

static inline pte_t*
mkptep_va(ptr_t vm_mnt, ptr_t vaddr)
{
    return (pte_t*)(vm_mnt & ~L0T_MASK) + pfn(vaddr);
}

static inline pte_t*
mkptep_pn(ptr_t vm_mnt, ptr_t pn)
{
    return (pte_t*)(vm_mnt & ~L0T_MASK) + (pn & L0T_MASK);
}

static inline pfn_t
pfn_at(ptr_t va, size_t lvl_size) {
    return __index(va) / lvl_size;
}


/**
 * @brief Shift the ptep such that it points to an
 *        immediate lower level of page table
 * 
 * @param ptep 
 * @return pte_t* 
 */
static inline pte_t*
ptep_step_into(pte_t* ptep)
{
    return __LnTEP_SHIFT_NEXT(ptep);
}

/**
 * @brief Shift the ptep such that it points to an
 *        immediate upper level of page table
 * 
 * @param ptep 
 * @return pte_t* 
 */
static inline pte_t*
ptep_step_out(pte_t* ptep)
{
    ptr_t unshifted = (ptr_t)mkptep_pn(VMS_SELF, ptep_pfn(ptep));
    return mkptep_va(_VM_OF(ptep), unshifted);
}

/**
 * @brief Make a L0TEP from given mnt and va
 * 
 * @param ptep 
 * @return pte_t* 
 */
static inline pte_t*
mkl0tep_va(ptr_t mnt, ptr_t va)
{
    return mkl0tep(mkptep_va(mnt, va));
}

static inline pte_t*
mkl1tep_va(ptr_t mnt, ptr_t va)
{
    return mkl1tep(mkptep_va(mnt, va));
}

static inline pte_t*
mkl2tep_va(ptr_t mnt, ptr_t va)
{
    return mkl2tep(mkptep_va(mnt, va));
}

static inline pte_t*
mkl3tep_va(ptr_t mnt, ptr_t va)
{
    return mkl3tep(mkptep_va(mnt, va));
}

static inline pte_t*
mklntep_va(int level, ptr_t mnt, ptr_t va)
{
    if (level == 0)
        return mkl0tep_va(mnt, va);
    
#if LnT_ENABLED(1)
    if (level == 1)
        return mkl1tep_va(mnt, va);
#endif

#if LnT_ENABLED(2)
    if (level == 2)
        return mkl2tep_va(mnt, va);
#endif

#if LnT_ENABLED(3)
    if (level == 3)
        return mkl3tep_va(mnt, va);
#endif
    
    return mkptep_va(mnt, va);
}

static inline unsigned long
lnt_page_size(int level)
{
    if (level == 0)
        return L0T_SIZE;
    if (level == 1)
        return L1T_SIZE;
    if (level == 2)
        return L2T_SIZE;
    if (level == 3)
        return L3T_SIZE;
    
    return LFT_SIZE;
}

static inline bool
pt_last_level(int level)
{
    return level == _PTW_LEVEL - 1;
}

static inline ptr_t
va_mntpoint(ptr_t va)
{
    return __vaddr(_VM_OF(va));
}

static inline unsigned int
va_level_index(ptr_t va, size_t lvl_size)
{
    return (va / lvl_size) & _PAGE_LEVEL_MASK;
}

static inline bool
lntep_implie(pte_t* ptep, ptr_t addr, size_t lvl_size)
{
    return ptep_va(ptep, lvl_size) == __vaddr(addr);
}

static inline bool
is_ptep(ptr_t addr)
{
    ptr_t mnt = va_mntpoint(addr);
    return mnt == VMS_MOUNT_1 || mnt == VMS_SELF;
}

static inline bool
vmnt_packed(pte_t* ptep)
{
    return is_ptep(__ptr(ptep));
}

static inline bool
active_vms(ptr_t vmnt)
{
    return vmnt == VMS_SELF;
}

static inline bool
lntep_implie_vmnts(pte_t* ptep, size_t lvl_size)
{
    return lntep_implie(ptep, VMS_SELF, lvl_size) ||
           lntep_implie(ptep, VMS_MOUNT_1, lvl_size);
}


static inline int
ptep_count_level(pte_t* ptep)
{
    int i = 0;
    ptr_t addr = (ptr_t)ptep;

    if (!is_ptep(addr << (LEVEL_SHIFT * i++)))
        return MAX_LEVEL - i;

#if LnT_ENABLED(1)
    if (!is_ptep(addr << (LEVEL_SHIFT * i++)))
        return MAX_LEVEL - i;
#endif

#if LnT_ENABLED(2)
    if (!is_ptep(addr << (LEVEL_SHIFT * i++)))
        return MAX_LEVEL - i;
#endif

#if LnT_ENABLED(3)
    if (!is_ptep(addr << (LEVEL_SHIFT * i++)))
        return MAX_LEVEL - i;
#endif
    
    return 0;
}

#endif /* __LUNAIX_PAGETABLE_H */
