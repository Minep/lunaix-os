#ifndef __LUNAIX_ASTM_H
#define __LUNAIX_ASTM_H
/**
 * Virtual Address Space Translation/Traverse Machinery (VASTM)
 */

#include <lunaix/mm/page.h>
#include <lunaix/status.h>
#include <lunaix/spike.h>
#include <lunaix/types.h>
#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/procvm.h>

#include <asm/mm_defs.h>

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
    return (pte_t*)(sanitize_vaddr(root.v) & PAGE_MASK);
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

static inline pte_t*
vastm_ptep_at_resolution(pte_t* root, ptr_t va, int res)
{
    if (res == RES_L0T)
        return ptep_at(root, va, L0T);

#if has_ptlevel(L1T) 
    else if (res == RES_L1T)
        return ptep_at(root, va, L1T);
#endif

#if has_ptlevel(L2T)
    else if (res == RES_L2T)
        return ptep_at(root, va, L2T);
#endif

#if has_ptlevel(L3T)
    else if (res == RES_L3T)
        return ptep_at(root, va, L3T);
#endif

    else if (res == RES_LFT)
        return ptep_at(root, va, LFT);
    else
        return NULL;
}

/**
 * Get the ptep of a specific entry along the path
 * in this level of page table represented by ptroot
 */
static inline pte_t*
vastm_ptep_from(ptroot_t root, ptr_t va)
{
    return vastm_ptep_at_resolution(
            vastm_ptroot_ptr(root), va, vastm_ptroot_res(root));
}

static inline pte_t*
vastm_table_get_or_make(pte_t* ptep, pte_t attr)
{
    struct ppage* page;
    pte_t* next_table;

    if (pte_huge(pte_at(ptep)))
        return NULL;

    next_table = ptep_next_table(ptep);
    if (next_table)
        return next_table;

    page = get_ppage(alloc_leaflet(PGPOL_PGTABLE));
    if (!page)
        return NULL;
    
    set_pte(ptep, pte_setpaddr(attr, ppage_addr(page)));
    return (pte_t*)ppage_va(page);
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
 * Walk along a given translation route, bou<S-Insert>nded by the 
 * translation tree resolution
 *
 * @param ptroot Root of translation tree
 * @param va Translation route
 * @param res Resolution, in terms of page order.
 */
static inline ptroot_t
vastm_walk_along(ptroot_t root, ptr_t va, int resolution)
{
    int cur_res = vastm_ptroot_res(root);
    pte_t* ptep = vastm_ptroot_ptr(root);
    pte_t* next_ptep;

    if (cur_res > resolution && cur_res == RES_L0T) {
        next_ptep = ptep_next_table(ptep_at(ptep, va, L0T));
        if (!next_ptep)
            goto done;

        ptep = next_ptep;
        cur_res -= level_width(L0T);
    }

#if has_ptlevel(L1T)
    if (cur_res > resolution && cur_res == RES_L1T) {
        next_ptep = ptep_next_table(ptep_at(ptep, va, L1T));
        if (!next_ptep)
            goto done;

        ptep = next_ptep;
        cur_res -= level_width(L1T);
    }
#endif

#if has_ptlevel(L2T)
    if (cur_res > resolution && cur_res == RES_L2T) {
        next_ptep = ptep_next_table(ptep_at(ptep, va, L2T));
        if (!next_ptep)
            goto done;

        ptep = next_ptep;
        cur_res -= level_width(L2T);
    }
#endif

#if has_ptlevel(L3T)
    if (cur_res > resolution && cur_res == RES_L3T) {
        next_ptep = ptep_next_table(ptep_at(ptep, va, L3T));
        if (!next_ptep)
            goto done;

        ptep = next_ptep;
        cur_res -= level_width(L3T);
    }
#endif
    
    assert(cur_res <= resolution);
done:
    return vastm_ptroot_at(ptep, cur_res);
}


static inline pte_t*
vastm_make_along(ptroot_t root, ptr_t va, int resolution, pte_t attr)
{
    int cur_res = vastm_ptroot_res(root);
    pte_t* ptep = vastm_ptroot_ptr(root);

    attr = pte_mkroot(attr);

    if (cur_res > resolution && cur_res == RES_L0T) {
        ptep = vastm_table_get_or_make(ptep_at(ptep, va, L0T), attr);
        if (!ptep)
            return NULL;
        cur_res -= level_width(L0T);
    }

#if has_ptlevel(L1T)
    if (cur_res > resolution && cur_res == RES_L1T) {
        ptep = vastm_table_get_or_make(ptep_at(ptep, va, L1T), attr);
        if (!ptep)
            return NULL;
        cur_res -= level_width(L1T);
    }
#endif

#if has_ptlevel(L2T)
    if (cur_res > resolution && cur_res == RES_L2T) {
        ptep = vastm_table_get_or_make(ptep_at(ptep, va, L2T), attr);
        if (!ptep)
            return NULL;
        cur_res -= level_width(L2T);
    }
#endif

#if has_ptlevel(L3T)
    if (cur_res > resolution && cur_res == RES_L3T) {
        ptep = vastm_table_get_or_make(ptep_at(ptep, va, L3T), attr);
        if (!ptep)
            return NULL;
        cur_res -= level_width(L3T);
    }
#endif

    assert(cur_res <= resolution);
    return vastm_ptep_at_resolution(ptep, va, cur_res);
}

static inline pte_t*
vastm_walk_ptep(ptroot_t root, ptr_t va, int resolution)
{
    ptroot_t r;

    r = vastm_walk_along(root, va, resolution);
    return vastm_ptep_from(r, va);
}

static inline pte_t*
vastm_walk_ptep_strict(ptroot_t root, ptr_t va, int resolution)
{
    ptroot_t r;

    r = vastm_walk_along(root, va, resolution);
    if (vastm_ptroot_res(r) != resolution)
        return NULL;

    return vastm_ptep_from(r, va);
}

enum vastm_action
{
    VASTM_CONTINUE = 0,
    VASTM_BREAK = 1,
};

struct vastm_state;
typedef enum vastm_action (*entry_hanlder_t)(struct vastm_state*, pte_t* ptep, void*);

struct vastm_cb {
    entry_hanlder_t tra_cb[PT_LEVEL];
    void* cb_data;
};


struct vastm
{
    struct vastm_cb cb;
    int err_like;
};

struct vastm_state
{
    ptr_t va;
    ptr_t va_end;
    int min_res;
    int cur_res;
    struct vastm* param;
};

static inline void
vastm_param_prepare(struct vastm* param, void* cb_data)
{
    param->cb = (struct vastm_cb){ .cb_data = cb_data };
    param->err_like = 0;
}

static inline void
vastm_param_cb_set_interims(struct vastm* param, entry_hanlder_t cb)
{
    param->cb.tra_cb[ASTM_L0T] = cb;
    param->cb.tra_cb[ASTM_L1T] = cb;
    param->cb.tra_cb[ASTM_L2T] = cb;
    param->cb.tra_cb[ASTM_L3T] = cb;
}

static inline void
vastm_param_cb_set_uniform(struct vastm* param, entry_hanlder_t cb)
{
    vastm_param_cb_set_interims(param, cb);
    param->cb.tra_cb[ASTM_LFT] = cb;
}

static inline void
vastm_param_cb_set(struct vastm* param, int level, entry_hanlder_t cb)
{
    param->cb.tra_cb[level] = cb;
}

static inline void
vastm_walk_flag_err(struct vastm_state* state, int err)
{
    state->param->err_like = err;
}

static inline enum vastm_action
vastm_walk_flag_complete(struct vastm_state* state)
{
    state->param->err_like = 1;
    return VASTM_BREAK;
}

static inline int
vastm_walk_errno(struct vastm* param) 
{
    return param->err_like < 0 ? param->err_like : 0;
}

void
vastm_visit_next(struct vastm_state state, pte_t* next_table);

void
vastm_walk(struct vastm* param, 
            ptroot_t root, ptr_t va_start, ptr_t va_end, int res);

static inline bool
vastm_helper_create_intrim_table(struct vastm_state *state, pte_t* ptep, pte_attr_t prot)
{
    pte_t* next_table, pte;
    struct leaflet* leaflet;
    
    leaflet = alloc_leaflet(PGPOL_PGTABLE);
    if (!leaflet) {
        vastm_walk_flag_err(state, ENOMEM);
        return false;
    }

    next_table = (pte_t*)leaflet_va(leaflet);
    set_pte(ptep, mkpte(leaflet_addr(leaflet), prot));
    return true;
}

#endif
