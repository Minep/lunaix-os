#ifndef __LUNAIX_ARCH_TLB_H
#define __LUNAIX_ARCH_TLB_H

#include <lunaix/compiler.h>
#include <lunaix/mm/procvm.h>
#include <lunaix/mm/physical.h>

/**
 * @brief Invalidate an entry of all address space
 * 
 * @param va 
 */
static inline void must_inline
__tlb_invalidate(ptr_t va) 
{
    asm volatile("invlpg (%0)" ::"r"(va) : "memory");
}

/**
 * @brief Invalidate an entry of an address space indetified
 *        by ASID
 * 
 * @param va 
 */
static inline void must_inline
__tlb_flush_asid(unsigned int asid, ptr_t va) 
{
    // not supported on x86_32
    asm volatile("invlpg (%0)" ::"r"(va) : "memory");
}

/**
 * @brief Invalidate an entry of global address space
 * 
 * @param va 
 */
static inline void must_inline
__tlb_flush_global(ptr_t va) 
{
    // not supported on x86_32
    asm volatile("invlpg (%0)" ::"r"(va) : "memory");
}

/**
 * @brief Invalidate an entire TLB
 * 
 * @param va 
 */
static inline void must_inline
__tlb_flush_all() 
{
    asm volatile(
        "movl %%cr3, %%eax\n"
        "movl %%eax, %%cr3"
        :::"eax"
    );
}

/**
 * @brief Invalidate an entire address space
 * 
 * @param va 
 */
static inline void must_inline
__tlb_flush_asid_all(unsigned int asid) 
{
    // not supported on x86_32
    __tlb_flush_all();
}


/**
 * @brief Invalidate entries of all address spaces
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
static inline void 
tlb_flush_range(ptr_t addr, unsigned int npages)
{
    for (unsigned int i = 0; i < npages; i++)
    {
        __tlb_invalidate(addr + i * PAGE_SIZE);
    }
}

/**
 * @brief Invalidate entries of an address space identified
 *        by ASID
 * 
 * @param asid 
 * @param addr 
 * @param npages 
 */
static inline void 
tlb_flush_asid_range(unsigned int asid, ptr_t addr, unsigned int npages)
{
    for (unsigned int i = 0; i < npages; i++)
    {
        __tlb_flush_asid(asid, addr + i * PAGE_SIZE);
    }
}

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

#endif /* __LUNAIX_VMTLB_H */
