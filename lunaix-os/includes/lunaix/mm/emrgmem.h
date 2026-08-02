#ifndef __LUNAIX_MM_EMRGMEM_H
#define __LUNAIX_MM_EMRGMEM_H

#include <lunaix/types.h>
#include <klibc/string.h>

void*
emalloc(size_t size);

static inline void*
emcalloc(size_t size, size_t count)
{
    void* __r;

    __r = emalloc(size * count);
    if (!__r)
        return NULL;

    memset(__r, 0, size * count);
    return __r;
}

void
emfree(void* ptr);

void 
emrgmem_init();
#endif
