#include <sys-generic/bootmem.h>
#include <lunaix/sections.h>
#include <lunaix/spike.h>

#define BOOTMEM_SIZE   (4 * 4096)

static reclaimable char bootmem_pool[BOOTMEM_SIZE];
static unsigned int pos;

_default void*
bootmem_alloc(unsigned int size)
{
    ptr_t res;

    res = __ptr(bootmem_pool) + pos;
    
    size = ROUNDUP(size, 4);
    pos += size;

    if (pos >= BOOTMEM_SIZE) {
        spin();
        unreachable;
    }

    return (void*)res;
}

_default void
bootmem_free(void* ptr)
{
    // not need to support, as they are all one-shot
    return;
}