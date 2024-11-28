#ifndef __LUNAIX_ARCH_TLB_H
#define __LUNAIX_ARCH_TLB_H

#include <lunaix/types.h>

#include <asm/aa64_mmu.h>
#include <asm/aa64_sysinst.h>

#define pack_va(asid, ttl, va)                \
        (((asid & 0xffffUL) << 48)            | \
         ((ttl  & 0b1111UL) << 44)            | \
         (pfn(va) & ((1UL << 44) - 1)))

#define pack_rva(asid, ttl, base, n, scale)   \
        (((asid    & 0xffffUL) << 48)         | \
         ((_MMU_TG & 0b11UL) << 46)           | \
         ((n       & 0x1fUL) << 39)           | \
         ((scale   & 0b11UL) << 37)           | \
         ((ttl     & 0b1111UL) << 44)         | \
         (pfn(base)& ((1UL << 37) - 1)))

/**
 * @brief Invalidate an entry of all address space
 * 
 * @param va 
 */
static inline void must_inline
__tlb_invalidate(ptr_t va) 
{
    sys_a1(tlbi_vaae1, pack_va(0, 0, va));
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
    sys_a1(tlbi_vae1, pack_va(asid, 0, va));
}

/**
 * @brief Invalidate an entry of global address space
 * 
 * @param va 
 */
static inline void must_inline
__tlb_flush_global(ptr_t va) 
{
    __tlb_flush_asid(0, va);
}

/**
 * @brief Invalidate an entire TLB
 * 
 * @param va 
 */
static inline void must_inline
__tlb_flush_all() 
{
    sys_a0(tlbi_alle1);
}

/**
 * @brief Invalidate an entire address space
 * 
 * @param va 
 */
static inline void must_inline
__tlb_flush_asid_all(unsigned int asid) 
{
    sys_a1(tlbi_aside1, pack_va(asid, 0, 0));
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
#ifdef _MMU_USE_OA52
    for (unsigned int i = 0; i < npages; i++)
    {
        __tlb_invalidate(addr + i * PAGE_SIZE);
    }
#else
    sys_a1(tlbi_rvaae1, pack_rva(0, 0, addr, npages, 0));
#endif
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
#ifdef _MMU_USE_OA52
    for (unsigned int i = 0; i < npages; i++)
    {
        __tlb_flush_asid(asid, addr + i * PAGE_SIZE);
    }
#else
    sys_a1(tlbi_rvae1, pack_rva(asid, 0, addr, npages, 0));
#endif
}

#include <asm-generic/tlb-shared.h>

#endif /* __LUNAIX_VMTLB_H */
