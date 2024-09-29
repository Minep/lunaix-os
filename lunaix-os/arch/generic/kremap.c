#include <lunaix/mm/pagetable.h>
#include <lunaix/sections.h>

#include <asm/boot_stage.h>
#include <asm/mm_defs.h>
#include <asm-generic/init_pagetable.h>

ptr_t boot_text
kpt_alloc_table(struct pt_alloc* alloc)
{
    if (alloc->index >= alloc->total) {
        return 0;
    }

    ptr_t next;

    next = alloc->base + alloc->index * PAGE_SIZE;
    alloc->index++;

    return next;
}

static bool boot_text
__walk(struct ptw_state* state, ptr_t addr, 
       unsigned long level_size, bool create)
{
    pte_t pte, *pt;
    int level, pt_index;
    unsigned long lsize;
    ptr_t  next_level;

    pt = state->root;
    level = 0;

    do {
        lsize    = lnt_page_size(level);
        pt_index = va_level_index(addr, lsize);

        pte = pt[pt_index];
        if (!pte_isnull(pte)) {
            next_level = pte_paddr(pte);
            goto cont;
        }

        if (pt_last_level(level) || lsize == level_size) {
            break;
        }

        if (!create) {
            goto fail;
        }

        next_level = kpt_alloc_table(state->alloc);
        if (!next_level) {
            state->lntp = NULL;
            goto fail;
        }

        pte = mkpte(next_level, KERNEL_PGTAB);
        pt[pt_index] = pte;

    cont:
        pt = (pte_t*) next_level;
        level++;
    }
    while (lsize > level_size);

    state->lntp  = pt;
    state->level = level;
    state->tab_index = pt_index;

    return true;

fail:
    state->lntp = NULL;
    return false;
}

unsigned int boot_text
kpt_set_ptes_flatten(struct ptw_state* state, ptr_t addr, 
                pte_t pte, unsigned long lsize, unsigned int nr)
{
    unsigned int tab_i, _n;
    pte_t *lntp;

    _n = 0;
    addr = addr & ~(lsize - 1);

    do {
        if (!__walk(state, addr, lsize, true)) {
            break;
        }

        lntp  = state->lntp;
        tab_i = state->tab_index;
        while (_n < nr && tab_i < LEVEL_SIZE) {
            lntp[tab_i++] = pte;
            pte = pte_advance(pte, lsize);

            addr += lsize;
            _n++;
        }
    }
    while (_n < nr);

    return _n;
}

#define ksection_maps autogen_name(ksecmap)
#define PF_X 0x1
#define PF_W 0x2

extern_autogen(ksecmap);

bridge_farsym(__kexec_text_start);
bridge_farsym(ksection_maps);

void boot_text
kpt_migrate_highmem(struct ptw_state* state)
{
    pte_t pte;
    struct ksecmap* maps;
    struct ksection* section;
    pfn_t pgs;

    maps = (struct ksecmap*)to_kphysical(__far(ksection_maps));

    for (unsigned int i = 0; i < maps->num; i++)
    {
        section = &maps->secs[i];
        
        if (section->va < KERNEL_RESIDENT) {
            continue;
        }

        pte = mkpte(section->pa, KERNEL_RDONLY);
        if ((section->flags & PF_X)) {
            pte = pte_mkexec(pte);
        }
        if ((section->flags & PF_W)) {
            pte = pte_mkwritable(pte);
        }

        pgs = leaf_count(section->size);
        kpt_set_ptes(state, section->va, pte, LFT_SIZE, pgs);
    }
}