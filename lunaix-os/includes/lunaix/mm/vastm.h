#ifndef __LUNAIX_ASTM_H
#define __LUNAIX_ASTM_H
/**
 * Virtual Address Space Translation/Traverse Machinery (VASTM)
 */

#include <lunaix/spike.h>
#include <lunaix/types.h>
#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/procvm.h>

#include <asm/vastm.h>

#define ASTM_L0T 0

#if LnT_ENABLED(1)
#define ASTM_L1T 1
#else
#define ASTM_L1T ASTM_L0T
#endif

#if LnT_ENABLED(2)
#define ASTM_L2T 2
#else
#define ASTM_L2T ASTM_L1T
#endif

#if LnT_ENABLED(3)
#define ASTM_L3T 3
#else
#define ASTM_L3T ASTM_L2T
#endif

#define ASTM_LFT ( PT_LEVEL - 1 )

#define RES_LFT         ( 0 )
#define RES_L3T         ( RES_LFT + _L3T_LEVEL_WIDTH )
#define RES_L2T         ( RES_L3T + _L2T_LEVEL_WIDTH )
#define RES_L1T         ( RES_L2T + _L1T_LEVEL_WIDTH )
#define RES_L0T         ( RES_L1T + _L0T_LEVEL_WIDTH ) 

enum vastm_action
{
    VASTM_CONTINUE = 0,
    VASTM_BREAK_LEVEL = 1,
    VASTM_DONE = 2,
    VASTM_RETRY = 3
};

typedef enum vastm_action (*entry_hanlder_t)(ptr_t va, pte_t* entry, void*);

struct vastm {
    entry_hanlder_t tra_cb[PT_LEVEL];
    void* cb_data;
};

struct ptroot;
typedef struct ptroot ptroot_t;


struct ptroot {
    unsigned long v;
};

/**
 * Get a pointer to page table represented
 * by ptroot
 */
static inline pte_t*
vastm_ptroot_ptr(ptroot_t root)
{
    return (pte_t*)phy_to_virt(__sanitize_vaddr(root.v) & PAGE_MASK);
}

/**
 * Get resolution in terms of page order of the ptroot
 * represented
 */
static inline int
vastm_ptroot_res(ptroot_t root)
{
    return root.v & ~PAGE_MASK;
}

/**
 * Convert a ptep at specific resolution into ptroot
 */
static inline ptroot_t
vastm_ptroot_at(pte_t* table, int resolution)
{
    return (ptroot_t){ ((ptr_t)table & PAGE_MASK) + (resolution & ~PAGE_MASK) };
}

static inline ptroot_t
vastm_ptroot(pte_t* table)
{
    return vastm_ptroot_at(table, RES_L0T);
}

/**
 * Get the ptep of a specific entry along the path
 * in this level of page table represented by ptroot
 */
static inline pte_t*
vastm_ptep_from(ptroot_t root, ptr_t va)
{
    int res;
    res = vastm_ptroot_res(root);

    if (res == RES_L0T)
        va = level_index(va, 0);
    else if (res == RES_L1T)
        va = level_index(va, 1);
    else if (res == RES_L2T)
        va = level_index(va, 2);
    else if (res == RES_L3T)
        va = level_index(va, 3);
    else if (res == RES_LFT)
        va = level_index(va, F);
    else
        return NULL;

    return &vastm_ptroot_ptr(root)[va];
}

/**
 * Get the currently activated translation tree
 * (i.e., the current virtual address space)
 */
static inline ptroot_t
vastm_current_root()
{
    return vastm_ptroot((pte_t*)phy_to_virt(current_vas()));
}


static inline ptroot_t
vastm_procvm_root(struct proc_mm* vms)
{
    return vastm_ptroot((pte_t*)phy_to_virt(vms->vmroot));
}

/**
 * Walk along a given translation route, bounded by the 
 * translation tree resolution
 *
 * @param ptroot Root of translation tree
 * @param va Translation route
 * @param res Resolution, in terms of page order.
 */
ptroot_t
vastm_walk_along(ptroot_t root, ptr_t va, int resolution, struct vastm* cb);

static inline pte_t*
vastm_walk_ptep(ptroot_t root, ptr_t va, int resolution)
{
    ptroot_t r;

    r = vastm_walk_along(root, va, resolution, NULL);
    return vastm_ptep_from(r, va);
}

bool
vastm_walk_between(ptroot_t root, ptr_t from, ptr_t to, int resolution, struct vastm* cb);

static inline bool
vastm_walk_until(ptroot_t root, ptr_t end, int resolution, struct vastm* cb)
{
    return vastm_walk_between(root, 0, end, resolution, cb);
}

static inline bool
vastm_walk_until_page(ptroot_t root, ptr_t end, struct vastm* cb)
{
    return vastm_walk_between(root, 0, end, PAGE_SHIFT, cb);
}

static inline int
__vastm_resolution_level(int res)
{
    if (res == RES_L0T)
        return ASTM_L0T;
    else if (res == RES_L1T)
        return ASTM_L1T;
    else if (res == RES_L2T)
        return ASTM_L2T;
    else if (res == RES_L3T)
        return ASTM_L3T;
    else if (res == RES_LFT)
        return ASTM_LFT;

    fail("invalid translation tree resolution value.");
}
#endif
