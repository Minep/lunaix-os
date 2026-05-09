#include <lunaix/mm/vastm.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#include <asm/mempart.h>
#include <asm/tlb.h>

static volatile ptr_t prev_va = VMAP;

struct vmap_state {
    int n;
    pte_t attr;
    ptr_t pa;
    ptr_t va;
};

static enum vastm_action
__vmap_create_interim(struct vastm_state *state, pte_t *entry, void *data)
{
    if (pte_isnull(pte_at(entry))) {
        vastm_helper_create_intrim_table(state, entry, KERNEL_PGTAB);
    } 

    vastm_visit_next(*state, ptep_next_table(entry));
    return VASTM_CONTINUE;
}

static inline int
__locate_vacant_slots(pte_t* ptep, int nr)
{
    int i = ptep_entry_index(ptep);
    int slots = nr;

    while (slots && i < LFT_LENGTH) {
        slots--;
        
        if (!pte_isnull(pte_at(ptep))) {
            slots = nr;
        }

        ptep++;
        i++;
    }

    if (slots)
        return -1;

    return nr;
}

static enum vastm_action
__setup_mappings(struct vastm_state *state, pte_t *entry, void *data)
{
    int offset;
    struct vmap_state* vmap_state;

    vmap_state = (struct vmap_state*)data;
    offset = __locate_vacant_slots(entry, vmap_state->n);

    if (offset < 0)
        return VASTM_BREAK;

    vmap_state->va = state->va + offset * PAGE_SIZE;
    set_ptes(&entry[offset], vmap_state->attr, vmap_state->pa, vmap_state->n);

    return vastm_walk_flag_complete(state);
}

ptr_t
vmap_ptes_at(pte_t pte, int n)
{
    struct vmap_state state;
    struct vastm param;
    
    state.n = n;
    state.pa = pte_paddr(pte);
    state.attr = pte_setpaddr(pte, 0);
    state.va = 0;
    
    vastm_param_prepare(&param, &state);
    vastm_param_cb_set_interims(&param, __vmap_create_interim);
    vastm_param_cb_set(&param, ASTM_LFT, __setup_mappings);
    
    vastm_walk(&param, vastm_current_root(), 
                prev_va, VMAP_END, RES_LFT);
    
    if (state.va) {
        prev_va = state.va >= VMAP_END ? VMAP : state.va;
    }
 
    return state.va;
}

void
vunmap_at(ptr_t vmap_addr, int n)
{
    pte_t* ptep;

    vmap_addr = vmap_addr & PAGE_MASK;
    
    ptep = vastm_walk_ptep_strict(vastm_current_root(), vmap_addr, RES_LFT);
    if (!ptep)
        return;

    while (n--)
        set_pte(ptep++, null_pte);

    tlb_flush_kernel_ranged(vmap_addr, n);
}
