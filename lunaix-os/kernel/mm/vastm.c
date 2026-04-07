#include <lunaix/mm/vastm.h>

static inline enum vastm_action
__invoke_vastm_action(struct vastm* cb, int level, ptr_t va, pte_t* ptep)
{
    entry_hanlder_t handler;
    
    if (!cb)
        return VASTM_CONTINUE;
    
    handler = cb->tra_cb[level];
    if (!handler)
        return VASTM_CONTINUE;

    return handler(va, ptep, cb->cb_data);
}

static inline ptroot_t
__walk_along_lft(pte_t* table, int target_res, ptr_t va, struct vastm* cb)
{
    pte_t* ptep;
    enum vastm_action next_step;

    if (target_res >= RES_LFT)
        return vastm_ptroot_at(table, target_res);
    
    ptep = &table[level_index(va, 0)];
    next_step = __invoke_vastm_action(cb, ASTM_LFT, va, ptep);
    
    return vastm_ptroot_at(table, RES_LFT);
}

static inline ptroot_t
__walk_along_l3t(pte_t* table, int target_res, ptr_t va, struct vastm* cb)
{
#if LnT_ENABLED(3)
    pte_t* ptep, pte;
    enum vastm_action next_step;

    if (target_res >= RES_L2T)
        return vastm_ptroot_at(table, target_res);
    
    ptep = &table[level_index(va, 0)];
    next_step = __invoke_vastm_action(cb, ASTM_L3T, va, ptep);
    
    if (next_step != VASTM_CONTINUE)
        return vastm_ptroot_at(table, RES_L3T);
    
    pte = pte_at(ptep);
    if (!pte_isloaded(pte) || pte_huge(pte))
        return vastm_ptroot_at(table, RES_L2T);
    
    table = (pte_t*)phy_to_virt(pte_paddr(pte));
#endif
    
    return __walk_along_lft(table, target_res, va, cb);
}

static inline ptroot_t
__walk_along_l2t(pte_t* table, int target_res, ptr_t va, struct vastm* cb)
{
#if LnT_ENABLED(2)
    pte_t* ptep, pte;
    enum vastm_action next_step;

    if (target_res >= RES_L2T)
        return vastm_ptroot_at(table, target_res);
    
    ptep = &table[level_index(va, 0)];
    next_step = __invoke_vastm_action(cb, ASTM_L2T, va, ptep);
    
    if (next_step != VASTM_CONTINUE)
        return vastm_ptroot_at(table, RES_L2T);

    pte = pte_at(ptep);
    if (!pte_isloaded(pte) || pte_huge(pte))
        return vastm_ptroot_at(table, RES_L2T);
    
    table = (pte_t*)phy_to_virt(pte_paddr(pte));
#endif

    return __walk_along_l3t(table, target_res, va, cb);
}

static inline ptroot_t
__walk_along_l1t(pte_t* table, int target_res, ptr_t va, struct vastm* cb)
{
#if LnT_ENABLED(1) 
    pte_t* ptep, pte;
    enum vastm_action next_step;

    if (target_res >= RES_L1T)
        return vastm_ptroot_at(table, target_res);
    
    ptep = &table[level_index(va, 0)];
    next_step = __invoke_vastm_action(cb, ASTM_L1T, va, ptep);
    
    if (next_step != VASTM_CONTINUE)
        return vastm_ptroot_at(table, RES_L1T);
    
    pte = pte_at(ptep);
    if (!pte_isloaded(pte) || pte_huge(pte))
        return vastm_ptroot_at(table, RES_L2T);
    
    table = (pte_t*)phy_to_virt(pte_paddr(pte));
#endif    

    return __walk_along_l2t(table, target_res, va, cb);
}

static inline ptroot_t
__walk_along_l0t(pte_t* table, int target_res, ptr_t va, struct vastm* cb)
{
    pte_t* ptep, pte;
    enum vastm_action next_step;

    if (target_res >= RES_L0T)
        return vastm_ptroot_at(table, target_res);
    
    ptep = &table[level_index(va, 0)];
    next_step = __invoke_vastm_action(cb, ASTM_L0T, va, ptep);
    
    if (next_step != VASTM_CONTINUE)
        return vastm_ptroot_at(table, RES_L0T);
    
    pte = pte_at(ptep);
    if (!pte_isloaded(pte) || pte_huge(pte))
        return vastm_ptroot_at(table, RES_L2T);
    
    table = (pte_t*)phy_to_virt(pte_paddr(pte));

    return __walk_along_l1t(table, target_res, va, cb);
}

ptroot_t
vastm_walk_along(ptroot_t root, ptr_t va, int resolution, struct vastm* cb)
{
    int res;
    pte_t* table_root;

    res = vastm_ptroot_res(root);
    table_root = vastm_ptroot_ptr(root);

    if (res == RES_L0T)
        return __walk_along_l0t(table_root, res, va, cb);
    if (res == RES_L1T)
        return __walk_along_l1t(table_root, res, va, cb);
    if (res == RES_L2T)
        return __walk_along_l2t(table_root, res, va, cb);
    if (res == RES_L3T)
        return __walk_along_l3t(table_root, res, va, cb);
    if (res == RES_LFT)
        return __walk_along_lft(table_root, res, va, cb);

    fail("invalid traversal tree resolution value");
}

// --------- //

static inline bool
__walk_between_lft(pte_t* table, ptr_t from, ptr_t to, int target_res, struct vastm* cb)
{
    pte_t *ptep, pte;
    int index;
    enum vastm_action next_step;

    if (target_res > RES_LFT)
        return true;

    if (from >= to)
        return true;
    
    index = level_index(from, LFT);
    
    while (index < L1T_LENGTH) {
        ptep = &table[index];

        next_step = __invoke_vastm_action(cb, ASTM_LFT, from, ptep);
        if (next_step == VASTM_DONE)
            return false;

        if (next_step == VASTM_BREAK_LEVEL)
            break;

        if (next_step == VASTM_RETRY)
            continue;
        
        index++;
        from += LFT_SIZE;
    }

    return true;
}

static inline bool
__walk_between_l3t(pte_t* table, ptr_t from, ptr_t to, int target_res, struct vastm* cb)
{
#if LnT_ENABLED(3)
    pte_t *ptep, pte, *next_table;
    int index;
    bool should_stop;
    enum vastm_action next_step;

    if (target_res > RES_L2T)
        return false;
    
    i = 0;
    should_stop = false;
    index = level_index(from, L3T);
    
    while (index < L3T_LENGTH && from < to) 
    {
        ptep = &table[index];
        pte = pte_at(ptep);

        if (pte_isloaded(pte) && !pte_huge(pte)) {
            next_table = (pte_t*)phy_to_virt(pte_paddr(pte));
            should_stop = !__walk_between_lft(next_table, from, to, target_res, cb);
        }

        next_step = __invoke_vastm_action(cb, ASTM_L3T, from, ptep);

        if (should_stop || next_step == VASTM_DONE)
            return false;

        if (next_step == VASTM_BREAK_LEVEL)
            break;

        if (next_step == VASTM_RETRY)
            continue;
        
        index++;
        from += L3T_SIZE;
    }

    return true;
#else
    return __walk_between_lft(table, from, to, target_res, cb);
#endif
}

static inline bool
__walk_between_l2t(pte_t* table, ptr_t from, ptr_t to, int target_res, struct vastm* cb)
{
#if LnT_ENABLED(2)
    pte_t *ptep, pte, *next_table;
    int index;
    bool should_stop;
    enum vastm_action next_step;

    if (target_res > RES_L2T)
        return false;
    
    should_stop = false;
    index = level_index(from, L2T);
    
    while (index < L2T_LENGTH) 
    {
        ptep = &table[index];
        pte = pte_at(ptep);

        if (pte_isloaded(pte) && !pte_huge(pte)) {
            next_table = (pte_t*)phy_to_virt(pte_paddr(pte));
            should_stop = !__walk_between_l3t(next_table, from, to, target_res, cb);
        }

        next_step = __invoke_vastm_action(cb, ASTM_L2T, from, ptep);

        if (should_stop || next_step == VASTM_DONE)
            return false;

        if (next_step == VASTM_BREAK_LEVEL)
            break;

        if (next_step == VASTM_RETRY)
            continue;
        
        index++;
        from += L2T_SIZE;
    }

    return true;
#else
    return __walk_between_l3t(table, from, to, target_res, cb);
#endif
}

static inline bool
__walk_between_l1t(pte_t* table, ptr_t from, ptr_t to, int target_res, struct vastm* cb)
{
#if LnT_ENABLED(1)
    pte_t *ptep, pte, *next_table;
    int index;
    bool should_stop;
    enum vastm_action next_step;

    if (target_res > RES_L1T)
        return false;

    should_stop = false;
    index = level_index(from, L1T);
    
    while (index < L1T_LENGTH && from < to) 
    {
        ptep = &table[index];
        pte = pte_at(ptep);

        if (pte_isloaded(pte) && !pte_huge(pte)) {
            next_table = (pte_t*)phy_to_virt(pte_paddr(pte));
            should_stop = !__walk_between_l2t(next_table, from, to, target_res, cb);
        }

        next_step = __invoke_vastm_action(cb, ASTM_L1T, from, ptep);

        if (should_stop || next_step == VASTM_DONE)
            return false;

        if (next_step == VASTM_BREAK_LEVEL)
            break;

        if (next_step == VASTM_RETRY)
            continue;
        
        index++;
        from += L1T_SIZE;
    }

    return true;
#else
    return __walk_between_l2t(table, from, to, target_res, cb);
#endif
}

static inline bool
__walk_between_l0t(pte_t* table, ptr_t from, ptr_t to, int target_res, struct vastm* cb)
{
    pte_t *ptep, pte, *next_table;
    int index;
    bool should_stop;
    enum vastm_action next_step;

    if (target_res > RES_L1T)
        return false;
    
    should_stop = false;
    index = level_index(from, L0T);
    
    while (index < L0T_LENGTH && from < to) 
    {
        ptep = &table[index];
        pte = pte_at(ptep);

        if (pte_isloaded(pte) && !pte_huge(pte)) {
            next_table = (pte_t*)phy_to_virt(pte_paddr(pte));
            should_stop = !__walk_between_l1t(next_table, from, to, target_res, cb);
        }

        next_step = __invoke_vastm_action(cb, ASTM_L0T, from, ptep);

        if (should_stop || next_step == VASTM_DONE)
            return false;

        if (next_step == VASTM_BREAK_LEVEL)
            break;

        if (next_step == VASTM_RETRY)
            continue;
        
        index++;
        from += L0T_SIZE;
    }

    return true;
}

bool
vastm_walk_between(ptroot_t root, ptr_t from, ptr_t to, int resolution, struct vastm* cb)
{

    int res;
    pte_t* table_root;
    enum vastm_action result;

    res = vastm_ptroot_res(root);
    table_root = vastm_ptroot_ptr(root);

    if (res == RES_L0T)
        result = __walk_between_l0t(table_root, from, to, resolution, cb);
    else if (res == RES_L1T)
        result = __walk_between_l1t(table_root, from, to, resolution, cb);
    else if (res == RES_L2T)
        result = __walk_between_l2t(table_root, from, to, resolution, cb);
    else if (res == RES_L3T)
        result = __walk_between_l3t(table_root, from, to, resolution, cb);
    else if (res == RES_LFT)
        result = __walk_between_lft(table_root, from, to, resolution, cb);
    else
        fail("invalid traversal tree resolution value");

    return result != VASTM_BREAK_LEVEL;
}

