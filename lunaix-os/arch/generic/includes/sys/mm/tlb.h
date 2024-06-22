#ifndef __LUNAIX_ARCH_TLB_H
#define __LUNAIX_ARCH_TLB_H

#include <lunaix/compiler.h>
#include <lunaix/mm/procvm.h>
#include <lunaix/mm/physical.h>

/**
 * @brief Invalidate entries of all address spaces
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
void 
tlb_flush_range(ptr_t addr, unsigned int npages);

/**
 * @brief Invalidate entries of an address space identified
 *        by ASID
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
void 
tlb_flush_asid_range(unsigned int asid, ptr_t addr, unsigned int npages);

/**
 * @brief Invalidate an entry of kernel address spaces
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
void 
tlb_flush_kernel(ptr_t addr);

/**
 * @brief Invalidate entries of kernel address spaces
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
void 
tlb_flush_kernel_ranged(ptr_t addr, unsigned int npages);

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

#endif /* __LUNAIX_VMTLB_H */
