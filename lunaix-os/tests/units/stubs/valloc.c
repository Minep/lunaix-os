#include <lunaix/mm/valloc.h>
#include <testing/memchk.h>
#include <stddef.h>

extern void *malloc(size_t);
extern void *calloc(size_t, size_t);
extern void free(void*);

static inline void*
_my_malloc(size_t size)
{
    void* ptr;

    ptr = malloc(size);
    memchk_log_alloc((unsigned long)ptr, size);
    
    return ptr;
}

static inline void*
_my_calloc(size_t size, int n)
{
    void* ptr;

    ptr = calloc(size, n);
    memchk_log_alloc((unsigned long)ptr, size * n);
    
    return ptr;
}

static inline void
_my_free(void* addr)
{
    memchk_log_free((unsigned long)addr);
}

void*
valloc(unsigned int size)
{
    return _my_malloc(size);
}

void*
vzalloc(unsigned int size)
{
    return _my_calloc(size, 1);
}

void*
vcalloc(unsigned int size, unsigned int count)
{
    return _my_calloc(size, count);
}

void
vfree(void* ptr)
{
    _my_free(ptr);
}

void
vfree_safe(void* ptr)
{
    if (ptr) _my_free(ptr);
}

void*
valloc_dma(unsigned int size)
{
    return _my_malloc(size);
}

void*
vzalloc_dma(unsigned int size)
{
    return _my_calloc(size, 1);
}

void
vfree_dma(void* ptr)
{
    _my_free(ptr);
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