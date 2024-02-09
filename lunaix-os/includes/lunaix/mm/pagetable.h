#ifndef __LUNAIX_PAGETABLE_H
#define __LUNAIX_PAGETABLE_H

struct __pte;
typedef struct __pte pte_t;

#define pte_unref(ptep) ((pte_t)*(ptep))

#include <sys/mm/pagetable.h>
#include <sys/cpu.h>

#define _L0TEP_AT(vm_mnt)       ( ((vm_mnt) | L0T_MASK) & ~LFT_MASK )
#define _L1TEP_AT(vm_mnt)       ( ((vm_mnt) | L0T_MASK) & ~L3T_MASK )
#define _L2TEP_AT(vm_mnt)       ( ((vm_mnt) | L0T_MASK) & ~L2T_MASK )
#define _L3TEP_AT(vm_mnt)       ( ((vm_mnt) | L0T_MASK) & ~L1T_MASK )
#define _LFTEP_AT(vm_mnt)       ( ((vm_mnt) | L0T_MASK) & ~L0T_MASK )

#define _VM_OF(ptep)            ( (ptr_t)(ptep) & ~L0T_MASK )
#define _VM_PFN_OF(ptep)        ( ((ptr_t)(ptep) & L0T_MASK) / sizeof(pte_t) )
#define VMS_SELF             VMS_MASK

#define __LnTI_OF(ptep, n)\
    (_VM_PFN_OF(ptep) * LFT_SIZE / L##n##T_SIZE)

#define __LnTEP(ptep, va, n)\
    ( (pte_t*)_L##n##TEP_AT(_VM_OF(ptep)) + (((va) & VMS_MASK) / L##n##T_SIZE) )

#define __LnTEP_OF(ptep, n)\
    ( (pte_t*)_L##n##TEP_AT(_VM_OF(ptep)) + __LnTI_OF(ptep, n))

#define __LnTEP_SHIFT_NEXT(ptep)\
    ( (pte_t*)(_VM_OF(ptep) | ((_VM_PFN_OF(ptep) * LFT_SIZE) & L0T_MASK)) )

#define _has_LnT(n) (L##n##T_SIZE != LFT_SIZE)
#define PAGETABLE_LEVEL(n) _has_LnT(n)

bool 
pagetable_alloc(pte_t* ptep, pte_t pte);

static inline ptr_t
ptep_vm_pfn(pte_t* ptep)
{
    return _VM_PFN_OF(ptep);
}

static inline ptr_t
ptep_vm_mnt(pte_t* ptep)
{
    return _VM_OF(ptep);
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
mkl1t(pte_t* l0t_ptep, ptr_t va)
{
#if _has_LnT(1)
    if (!l0t_ptep) {
        return NULL;
    }

    pte_t pte = pte_unref(l0t_ptep);
    
    if (pte_isleaf(pte)) {
        return l0t_ptep;
    }
    
    return pagetable_alloc(l0t_ptep, pte) ? __LnTEP(l0t_ptep, va, 1) 
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
mkl2t(pte_t* l1t_ptep, ptr_t va)
{
#if _has_LnT(2)
    if (!l1t_ptep) {
        return NULL;
    }

    pte_t pte = pte_unref(l1t_ptep);
    
    if (pte_isleaf(pte)) {
        return l1t_ptep;
    }

    return pagetable_alloc(l1t_ptep, pte) ? __LnTEP(l1t_ptep, va, 2) 
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
mkl3t(pte_t* l2t_ptep, ptr_t va)
{
#if _has_LnT(3)
    if (!l2t_ptep) {
        return NULL;
    }
 
    pte_t pte = pte_unref(l2t_ptep);
    
    if (pte_isleaf(pte)) {
        return l2t_ptep;
    }

    return pagetable_alloc(l2t_ptep, pte) ? __LnTEP(l2t_ptep, va, 3) 
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
mklft(pte_t* l3t_ptep, ptr_t va)
{
    if (!l3t_ptep) {
        return NULL;
    }

    pte_t pte = pte_unref(l3t_ptep);
    
    if (pte_isleaf(pte)) {
        return l3t_ptep;
    }

    return pagetable_alloc(l3t_ptep, pte) ? __LnTEP(l3t_ptep, va, F) 
                                          : NULL;
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
    return addr / PAGE_SIZE;
}

static inline size_t
page_count(size_t size) {
    return (size + PAGE_MASK) / PAGE_SIZE;
}

static inline unsigned int
va_offset(ptr_t addr) {
    return addr & PAGE_MASK;
}

static inline ptr_t
page_addr(ptr_t pfn) {
    return pfn * PAGE_SIZE;
}

static inline ptr_t
va_align(ptr_t va) {
    return va & ~PAGE_MASK;
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
    return va / lvl_size;
}

#endif /* __LUNAIX_PAGETABLE_H */
