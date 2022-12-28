#ifndef __LUNAIX_MMAP_H
#define __LUNAIX_MMAP_H

#include <lunaix/fs.h>
#include <lunaix/mm/region.h>
#include <lunaix/types.h>

void*
mem_map(ptr_t pd_ref,
        vm_regions_t* regions,
        void* addr,
        struct v_file* file,
        off_t offset,
        size_t length,
        u32_t attrs,
        u32_t options);

void*
mem_unmap(ptr_t mnt, vm_regions_t* regions, void* addr, size_t length);

#endif /* __LUNAIX_MMAP_H */
