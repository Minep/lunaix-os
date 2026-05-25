#ifndef __LUNAIX_MM_PMALLOC
#define __LUNAIX_MM_PMALLOC

#include <lunaix/mm/pgpol.h>
#include <lunaix/mm/physical.h>

typedef unsigned int ppage_type_t;

struct pmalloc_pol;
struct pmpool;

struct pm_allocator { };

struct pmpool_ops
{
    struct ppage* (*alloc_page)(int order, struct pmalloc_pol* pol);
    void (*free_page)(struct pmpool* pool, struct ppage* page);
};


struct pmpool
{
    int type;
    struct ppage* first;
    struct ppage* last;
    struct pmpool_ops ops;

    union {
        struct pm_allocator alloc_private[0];
        unsigned char __pad[128];
    };
};

struct pmalloc_pol
{
    struct pmpool* src_pool;
    bool should_zero;
    bool max_attempt;
    bool panic_on_fail;
    bool should_align;
    
    pgpol_t policy;
};

enum pmalloc_backend
{
    PMALLOC_NCONTIG,
    PMALLOC_BUDDY,
    PMALLOC_SIMPLE,
    NR_PMALLOC_BACKEND
};

#ifdef CONFIG_PMALLOC_METHOD_NCONTIG
void pmalloc_ncontig_initpool(struct pmpool* pool);
#endif
#ifdef CONFIG_PMALLOC_METHOD_BUDDY
void pmalloc_buddy_initpool(struct pmpool* pool);
#endif
#ifdef CONFIG_PMALLOC_METHOD_SIMPLE
void pmalloc_simple_initpool(struct pmpool* pool);
#endif

#endif
