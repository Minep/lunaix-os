#ifndef __LUNAIX_TLB_SHARED_H
#define __LUNAIX_TLB_SHARED_H

#include <lunaix/types.h>
#include <lunaix/mm/procvm.h>

/**
 * @brief Invalidate an entry of kernel address spaces
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
static inline void 
tlb_flush_kernel(ptr_t addr)
{
    __tlb_flush_global(addr);
}

/**
 * @brief Invalidate entries of kernel address spaces
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
static inline void 
tlb_flush_kernel_ranged(ptr_t addr, unsigned int npages)
{
    for (unsigned int i = 0; i < npages; i++)
    {
        tlb_flush_kernel(addr + i * PAGE_SIZE);
    }
}

/**
 * @brief Invalidate an entry within a process memory space
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
void
tlb_flush_mm(struct proc_mm* mm, ptr_t addr);

/**
 * @brief Invalidate entries within a process memory space
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
void
tlb_flush_mm_range(struct proc_mm* mm, ptr_t addr, unsigned int npages);

/**
 * @brief Invalidate an entry within a vm region
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
void
tlb_flush_vmr(struct mm_region* vmr, ptr_t va);

/**
 * @brief Invalidate all entries within a vm region
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
void
tlb_flush_vmr_all(struct mm_region* vmr);

/**
 * @brief Invalidate entries within a vm region
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
void
tlb_flush_vmr_range(struct mm_region* vmr, ptr_t addr, unsigned int npages);

#endif /* __LUNAIX_TLB_SHARED_H */
