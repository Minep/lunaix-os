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
    void (*log_boottime_summary)(struct pmpool* pool);

    int (*reserve_range)(struct pmpool*, 
                            unsigned long start, unsigned long span);
    int (*unreserve_range)(struct pmpool*, 
                            unsigned long start, unsigned long span);
};

#define PMPOOl_ALLOC_PRIVATE_RSVD       128
#ifdef CONFIG_PMALLOC_METHOD_BUDDY
#undef PMPOOl_ALLOC_PRIVATE_RSVD
#define PMPOOl_ALLOC_PRIVATE_RSVD       512
#endif

struct pmpool
{
    union {
        int type;
        int id;
    };
    struct ppage* first;
    struct ppage* last;
    struct pmpool_ops ops;

    union {
        struct pm_allocator alloc_private[0];
        unsigned char __pad[PMPOOl_ALLOC_PRIVATE_RSVD];
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
