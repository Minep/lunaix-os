#include <lunaix/mm/valloc.h>
#include <stddef.h>

extern void *malloc(size_t);
extern void *calloc(size_t, size_t);
extern void free(void*);

void*
valloc(unsigned int size)
{
    return malloc(size);
}

void*
vzalloc(unsigned int size)
{
    return calloc(size, 1);
}

void*
vcalloc(unsigned int size, unsigned int count)
{
    return calloc(size, count);
}

void
vfree(void* ptr)
{
    free(ptr);
}

void
vfree_safe(void* ptr)
{
    if (ptr) free(ptr);
}

void*
valloc_dma(unsigned int size)
{
    return malloc(size);
}

void*
vzalloc_dma(unsigned int size)
{
    return calloc(size, 1);
}

void
vfree_dma(void* ptr)
{
    free(ptr);
}

void
valloc_init()
{
    return;
}

void 
valloc_ensure_valid(void* ptr)
{
    return;
}