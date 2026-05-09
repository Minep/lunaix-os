#include <lunaix/sections.h>
#include <lunaix/compiler.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/pagetable.h>

#include <asm/boot_stage.h>
#include <asm/mm_defs.h>
#include <asm/pagetable.h>
#include <asm-generic/init_pagetable.h>
#include <asm-generic/boot_stage.h>

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

static inline pte_t*
__get_next_level(struct ptw_state* state, pte_t* ptep)
{
    pte_t pte, *next;
    
    pte = pte_at(ptep);
    next = (pte_t*)pte_paddr(pte);

    if (!next) {
        next = (pte_t*)kpt_alloc_table(state->alloc);
        set_pte(ptep, mkpte_root((ptr_t)next, KERNEL_PGTAB));
    }

    return next;
}

static inline void must_inline
__walk_until(struct ptw_state* state, ptr_t addr, unsigned long stop_lvl)
{
    pte_t *table, *ptep;

    table = (pte_t*)state->root;
    
    ptep = &table[level_index(addr, L0T)];
    if (level_size(L0T) <= stop_lvl || pte_huge(pte_at(ptep))) {
        goto done;
    }

#if has_ptlevel(L1T)
    table = __get_next_level(state, ptep);
    ptep = &table[level_index(addr, L1T)];
    if (level_size(L1T) <= stop_lvl || pte_huge(pte_at(ptep))) {
        goto done;
    }
#endif

#if has_ptlevel(L2T)
    table = __get_next_level(state, ptep);
    ptep = &table[level_index(addr, L2T)];
    if (level_size(L2T) <= stop_lvl || pte_huge(pte_at(ptep))) {
        goto done;
    }
#endif

#if has_ptlevel(L3T)
    table = __get_next_level(state, ptep);
    ptep = &table[level_index(addr, L3T)];
    if (level_size(L3T) <= stop_lvl || pte_huge(pte_at(ptep))) {
        goto done;
    }
#endif

    table = __get_next_level(state, ptep);
    ptep = &table[level_index(addr, LFT)];

done:
    state->lntp = ptep;
}

unsigned int boot_text
kpt_set_ptes_flatten(struct ptw_state* state, ptr_t addr, 
                pte_t pte, unsigned long lsize, unsigned int nr)
{
    unsigned int tab_i, _n, level_len;
    pte_t *lntp;

    _n = 0;
    addr = addr & ~(lsize - 1);

    level_len = lsize >= L0T_SIZE ? level_length(L0T) : 
                lsize >= L1T_SIZE ? level_length(L1T) :
                lsize >= L2T_SIZE ? level_length(L2T) :
                lsize >= L3T_SIZE ? level_length(L3T) :
                level_length(LFT);

    do {
        __walk_until(state, addr, lsize);

        lntp  = state->lntp;
        tab_i = ptep_entry_index(lntp);
        
        if (!pte_isnull(pte_at(lntp)))
            spin();

        while (_n < nr && tab_i < level_len) {
            set_pte(lntp, pte);

            pte = pte_setpaddr(pte, pte_paddr(pte) + lsize);
            addr += lsize;
            _n++;
        }
    } while (_n < nr);

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

        pgs = count_pages(section->size);
        kpt_set_ptes(state, section->va, pte, LFT_SIZE, pgs);
    }
}
