#ifndef __LUNAIX_MMAP_H
#define __LUNAIX_MMAP_H

#include <lunaix/fs.h>
#include <lunaix/mm/region.h>
#include <lunaix/types.h>

#define MEM_FLUSH_MSYNC_MASK         0xffff
#define MEM_FLUSH_UNMAP              ( 1 << 16 )

struct mmap_param
{
    struct proc_mm* pvms; // process vm
    off_t offset;         // mapped file offset
    size_t mlen;          // mapped memory length
    size_t flen;          // mapped file length
    u32_t proct;          // protections
    u32_t flags;          // other options
    u32_t type;           // region type
    ptr_t range_start;
    ptr_t range_end;
};

int
mem_adjust_inplace(vm_regions_t* regions,
                   struct mm_region* region,
                   ptr_t newend);

int 
mmap_user(void** addr_out,
        struct mm_region** created,
        ptr_t addr,
        struct v_file* file,
        struct mmap_param* param);

int
mem_map(void** addr_out,
        struct mm_region** created,
        ptr_t addr,
        struct v_file* file,
        struct mmap_param* param);

int
mem_unmap(vm_regions_t* regions, ptr_t addr, size_t length);

void
mem_unmap_region(struct mm_region* region);

void
mem_flush_pages(struct mm_region* region, ptr_t start, ptr_t end, int options);

int
mem_msync(vm_regions_t* regions, ptr_t addr, size_t length, int options);

#endif /* __LUNAIX_MMAP_H */
