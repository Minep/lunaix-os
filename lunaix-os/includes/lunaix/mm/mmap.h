#ifndef __LUNAIX_MMAP_H
#define __LUNAIX_MMAP_H

#include <lunaix/fs.h>
#include <lunaix/mm/region.h>
#include <lunaix/types.h>

struct mmap_param
{
    ptr_t vms_mnt;
    vm_regions_t* regions;
    off_t offset;
    size_t length;
    u32_t proct;
    u32_t flags;
    u32_t type;
};

int
mem_map(void** addr_out,
        struct mm_region** created,
        void* addr,
        struct v_file* file,
        struct mmap_param* param);

int
mem_unmap(ptr_t mnt, vm_regions_t* regions, void* addr, size_t length);

void
mem_sync_pages(ptr_t mnt,
               struct mm_region* region,
               ptr_t start,
               ptr_t length,
               int options);

int
mem_msync(ptr_t mnt,
          vm_regions_t* regions,
          ptr_t addr,
          size_t length,
          int options);

#endif /* __LUNAIX_MMAP_H */
