#ifndef __LUNAIX_MMAP_H
#define __LUNAIX_MMAP_H

#include <lunaix/fs.h>
#include <lunaix/types.h>

void*
mem_map(ptr_t pd_ref,
        struct llist_header* regions,
        void* addr,
        struct v_file* file,
        u32_t offset,
        size_t length,
        u32_t attrs);

void*
mem_unmap(ptr_t mnt, struct llist_header* regions, void* addr, size_t length);

#endif /* __LUNAIX_MMAP_H */
