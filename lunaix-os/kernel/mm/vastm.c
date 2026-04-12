#include <lunaix/mm/pagetable.h>
#include <lunaix/mm/vastm.h>

static inline void
__visit_at_lft(struct vastm_state state, pte_t* table)
{
    int i;
    entry_hanlder_t cb;
    enum vastm_action result;

    state.cur_res = RES_LFT;
    i = level_index(state.va, LFT);
    
    while (state.va < state.va_end && i < LFT_LENGTH)
    {
        cb = state.param->cb.tra_cb[ASTM_LFT];
        if (!cb) {
            goto cont;
        } 

        result = cb(&state, &table[i], state.param->cb.cb_data);
        if (state.param->err_like || result != VASTM_CONTINUE)
            break;
cont:
        i++;
        state.va = (state.va + LFT_SIZE) & LFT_MASK;
    }
}

static inline void
__visit_at_l3t(struct vastm_state state, pte_t* table)
{
#if has_ptlevel(3)
    int i;
    entry_hanlder_t cb;
    enum vastm_action result;

    state.cur_res = RES_L3T;
    i = level_index(state.va, L3T);
    
    while (state.va < state.va_end && i < L3T_LENGTH)
    {
        cb = state.param->cb.tra_cb[ASTM_L3T];
        if (!cb) {
            vastm_visit_next(state, ptep_next_table(&table[i]));
            goto cont;
        } 

        result = cb(&state, &table[i], state.param->cb.cb_data);
        if (state.param->err_like || result != VASTM_CONTINUE)
            break;
cont:
        i++;
        state.va = (state.va + L3T_SIZE) & L3T_MASK;
    }
#else
    __visit_at_lft(state, table);
#endif
}

static inline void
__visit_at_l2t(struct vastm_state state, pte_t* table)
{
#if has_ptlevel(2)
    int i;
    entry_hanlder_t cb;
    enum vastm_action result;

    state.cur_res = RES_L2T;
    i = level_index(state.va, L2T);
    
    while (state.va < state.va_end && i < L2T_LENGTH)
    {
        cb = state.param->cb.tra_cb[ASTM_L2T];
        if (!cb) {
            vastm_visit_next(state, ptep_next_table(&table[i]));
            goto cont;
        } 

        result = cb(&state, &table[i], state.param->cb.cb_data);
        if (state.param->err_like || result != VASTM_CONTINUE)
            break;
cont:
        i++;
        state.va = (state.va + L2T_SIZE) & L2T_MASK;
    }
#else
    __visit_at_l3t(state, table);
#endif
}


static inline void
__visit_at_l1t(struct vastm_state state, pte_t* table)
{
#if has_ptlevel(1)
    int i;
    entry_hanlder_t cb;
    enum vastm_action result;

    state.cur_res = RES_L1T;
    i = level_index(state.va, L1T);
    
    while (state.va < state.va_end && i < L1T_LENGTH)
    {
        cb = state.param->cb.tra_cb[ASTM_L1T];
        if (!cb) {
            vastm_visit_next(state, ptep_next_table(&table[i]));
            goto cont;
        } 

        result = cb(&state, &table[i], state.param->cb.cb_data);
        if (state.param->err_like || result != VASTM_CONTINUE)
            break;
cont:
        i++;
        state.va = (state.va + L1T_SIZE) & L1T_MASK;
    }
#else
    __visit_at_l2t(state, table);
#endif
}


static inline void
__visit_at_l0t(struct vastm_state state, pte_t* table)
{
    int i;
    entry_hanlder_t cb;
    enum vastm_action result;

    state.cur_res = RES_L0T;
    i = level_index(state.va, L0T);
    
    while (state.va < state.va_end && i < L0T_LENGTH)
    {
        cb = state.param->cb.tra_cb[ASTM_L0T];
        if (!cb) {
            vastm_visit_next(state, ptep_next_table(&table[i]));
            goto cont;
        } 

        result = cb(&state, &table[i], state.param->cb.cb_data);
        if (state.param->err_like || result != VASTM_CONTINUE)
            break;
cont:
        i++;
        state.va = (state.va + L0T_SIZE) & L0T_MASK;
    }
}

void
vastm_visit_next(struct vastm_state state, pte_t* next_table)
{
    if (!next_table)
        return;
    
    if (state.cur_res <= state.min_res)
        return;

    if (state.cur_res == RES_L0T)
        return __visit_at_l1t(state, next_table);

    if (state.cur_res == RES_L1T)
        return __visit_at_l2t(state, next_table);

    if (state.cur_res == RES_L2T)
        return __visit_at_l3t(state, next_table);

    if (state.cur_res == RES_L3T)
        return __visit_at_lft(state, next_table);

    fail("invalid resolution");
}

void
vastm_walk(struct vastm* param, 
            ptroot_t root, ptr_t va_start, ptr_t va_end, int res)
{
    struct vastm_state state;
    state.param = param;
    state.va = va_start;
    state.va_end = va_end;
    state.min_res = res;
    state.cur_res = vastm_ptroot_res(root);

    if (state.cur_res == RES_L0T)
        return __visit_at_l0t(state, vastm_ptroot_ptr(root));

    if (state.cur_res == RES_L1T)
        return __visit_at_l1t(state, vastm_ptroot_ptr(root));

    if (state.cur_res == RES_L2T)
        return __visit_at_l2t(state, vastm_ptroot_ptr(root));

    if (state.cur_res == RES_L3T)
        return __visit_at_l3t(state, vastm_ptroot_ptr(root));

    if (state.cur_res == RES_LFT)
        return __visit_at_lft(state, vastm_ptroot_ptr(root));
    
    fail("invalid resolution");
}

