#include <klibc/string.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#define CLASS_LEN(class) (sizeof(class) / sizeof(class[0]))

// threshold to use external cake metadata
#define EXTERN_THRESHOLD    128

static char piles_names[][PILE_NAME_MAXLEN] = 
{
    "valloc_8",   "valloc_16",  "valloc_32",  "valloc_64",
    "valloc_128", "valloc_256", "valloc_512", "valloc_1k",
    "valloc_2k",  "valloc_4k",  "valloc_8k"  
};

#define M128    (4)
#define M1K     (M128 + 3)

static int page_counts[] = 
{
    [0] = 1,
    [1] = 1,
    [2] = 1,
    [3] = 1,
    [M128    ] = 1,
    [M128 + 1] = 2,
    [M128 + 2] = 2,
    [M1K     ] = 4,
    [M1K + 1 ] = 4,
    [M1K + 2 ] = 8,
    [M1K + 3 ] = 8
};

static char piles_names_dma[][PILE_NAME_MAXLEN] = 
{
    "valloc_dma_128", "valloc_dma_256", "valloc_dma_512",
    "valloc_dma_1k",  "valloc_dma_2k",  "valloc_dma_4k"
};

static struct cake_pile* piles[CLASS_LEN(piles_names)];
static struct cake_pile* piles_dma[CLASS_LEN(piles_names_dma)];

void
valloc_init()
{
    int opts = 0;
    for (size_t i = 0; i < CLASS_LEN(piles_names); i++) {
        int size = 1 << (i + 3);
        if (size >= EXTERN_THRESHOLD) {
            opts |= PILE_FL_EXTERN;
        }
        piles[i] = cake_new_pile(piles_names[i], size, page_counts[i], opts);
    }

    opts = PILE_ALIGN_CACHE;
    // DMA 内存保证128字节对齐
    for (size_t i = 0; i < CLASS_LEN(piles_names_dma); i++) {
        int size = 1 << (i + 7);
        if (size >= EXTERN_THRESHOLD) {
            opts |= PILE_FL_EXTERN;
        }
        piles_dma[i] = cake_new_pile(
            piles_names_dma[i], size, page_counts[M128 + i], opts);
    }
}

void*
__valloc(unsigned int size,
         struct cake_pile** segregate_list,
         size_t len,
         size_t boffset)
{
    size_t i = ilog2(size);
    i += (size - (1 << i) != 0);
    i -= boffset;

    if (i >= len)
        i = 0;

    return cake_grab(segregate_list[i]);
}

void
__vfree(void* ptr, struct cake_pile** segregate_list, size_t len)
{
    size_t i = 0;
    for (; i < len; i++) {
        if (cake_release(segregate_list[i], ptr)) {
            return;
        }
    }
}

void*
valloc(unsigned int size)
{
    return __valloc(size, piles, CLASS_LEN(piles_names), 3);
}

void*
vzalloc(unsigned int size)
{
    void* ptr = __valloc(size, piles, CLASS_LEN(piles_names), 3);
    memset(ptr, 0, size);
    return ptr;
}

void*
vcalloc(unsigned int size, unsigned int count)
{
    unsigned int alloc_size;
    if (umul_of(size, count, &alloc_size)) {
        return 0;
    }

    void* ptr = __valloc(alloc_size, piles, CLASS_LEN(piles_names), 3);
    memset(ptr, 0, alloc_size);
    return ptr;
}

void
vfree(void* ptr)
{
    __vfree(ptr, piles, CLASS_LEN(piles_names));
}

void
vfree_safe(void* ptr)
{
    if (!ptr) {
        return;
    }

    __vfree(ptr, piles, CLASS_LEN(piles_names));
}

void*
valloc_dma(unsigned int size)
{
    return __valloc(size, piles_dma, CLASS_LEN(piles_names_dma), 7);
}

void*
vzalloc_dma(unsigned int size)
{
    void* ptr = __valloc(size, piles_dma, CLASS_LEN(piles_names_dma), 7);
    memset(ptr, 0, size);
    return ptr;
}

void
vfree_dma(void* ptr)
{
    __vfree(ptr, piles_dma, CLASS_LEN(piles_names_dma));
}

inline void must_inline
valloc_ensure_valid(void* ptr) {
    cake_ensure_valid(ptr);
}