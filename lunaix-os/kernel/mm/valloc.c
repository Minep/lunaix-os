#include <lunaix/mm/cake.h>

#define MAX_CLASS 6

static char piles_names[MAX_CLASS][PILE_NAME_MAXLEN] = {
    "valloc_128", "valloc_256", "valloc_512",
    "valloc_1k",  "valloc_2k",  "valloc_4k"
};

static struct cake_pile* piles[MAX_CLASS];

void
valloc_init()
{
    for (size_t i = 0; i < MAX_CLASS; i++) {
        int size = 1 << (i + 7);
        piles[i] = cake_new_pile(&piles_names[i], size, size > 1024 ? 8 : 1);
    }
}

void*
valloc(unsigned int size)
{
    size_t i = 0;
    for (; i < MAX_CLASS; i++) {
        if (piles[i]->piece_size > size) {
            goto found_class;
        }
    }

    return NULL;

found_class:
    return cake_grab(piles[i]);
}

void
vfree(void* ptr)
{
    size_t i = 0;
    for (; i < MAX_CLASS; i++) {
        if (cake_release(piles[i], ptr)) {
            return;
        }
    }
}