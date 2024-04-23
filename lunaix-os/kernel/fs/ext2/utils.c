#include <lunaix/mm/valloc.h>
#include <lunaix/mm/page.h>


#include "utils.h"

void*
__alloc_block_buffer(struct v_superblock* vsb)
{
    size_t blksz = fsapi_block_size(vsb);
    if (blksz == PAGE_SIZE) {
        return vmalloc_page(0);
    }

    return valloc(blksz);
}

void
__free_block_buffer(struct v_superblock* vsb, void* buf)
{
    size_t blksz = fsapi_block_size(vsb);
    if (blksz == PAGE_SIZE) {
        vmfree(buf);
    }
    else {
        vfree(buf);
    }
}
