#include <klibc/string.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>

#define MAX_CLASS 6

static char piles_names[MAX_CLASS][PILE_NAME_MAXLEN] = {
    "valloc_16",  "valloc_32",  "valloc_64",
    "valloc_128", "valloc_256", "valloc_512"
};

static char piles_names_dma[MAX_CLASS][PILE_NAME_MAXLEN] = {
    "valloc_dma_128", "valloc_dma_256", "valloc_dma_512",
    "valloc_dma_1k",  "valloc_dma_2k",  "valloc_dma_4k"
};

static struct cake_pile* piles[MAX_CLASS];
static struct cake_pile* piles_dma[MAX_CLASS];

void
valloc_init()
{
    for (size_t i = 0; i < MAX_CLASS; i++) {
        int size = 1 << (i + 4);
        piles[i] = cake_new_pile(&piles_names[i], size, 1, 0);
    }

    // DMA 内存保证128字节对齐
    for (size_t i = 0; i < MAX_CLASS; i++) {
        int size = 1 << (i + 7);
        piles_dma[i] = cake_new_pile(
          &piles_names_dma[i], size, size > 1024 ? 8 : 1, PILE_CACHELINE);
    }
}

void*
__valloc(unsigned int size, struct cake_pile** segregate_list)
{
    size_t i = 0;
    for (; i < MAX_CLASS; i++) {
        if (segregate_list[i]->piece_size >= size) {
            goto found_class;
        }
    }

    return NULL;

found_class:
    return cake_grab(segregate_list[i]);
}

void
__vfree(void* ptr, struct cake_pile** segregate_list)
{
    size_t i = 0;
    for (; i < MAX_CLASS; i++) {
        if (cake_release(segregate_list[i], ptr)) {
            return;
        }
    }
}

void*
valloc(unsigned int size)
{
    return __valloc(size, &piles);
}

void*
vzalloc(unsigned int size)
{
    void* ptr = __valloc(size, &piles);
    memset(ptr, 0, size);
    return ptr;
}

void*
vcalloc(unsigned int size, unsigned int count)
{
    unsigned int alloc_size;
    if (__builtin_umul_overflow(size, count, &alloc_size)) {
        return 0;
    }

    void* ptr = __valloc(alloc_size, &piles);
    memset(ptr, 0, alloc_size);
    return ptr;
}

void
vfree(void* ptr)
{
    __vfree(ptr, &piles);
}

void*
valloc_dma(unsigned int size)
{
    return __valloc(size, &piles_dma);
}

void*
vzalloc_dma(unsigned int size)
{
    void* ptr = __valloc(size, &piles_dma);
    memset(ptr, 0, size);
    return ptr;
}

void
vfree_dma(void* ptr)
{
    __vfree(ptr, &piles_dma);
}