#include <lunaix/mm/page.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>

#include <asm/mempart.h>

static ptr_t start = VMAP;
static volatile ptr_t prev_va = 0;

void
vmap_set_start(ptr_t start_addr) {
    start = start_addr;
}

static pte_t*
__alloc_contig_ptes(pte_t* ptep, size_t base_sz, int n)
{
    int _n     = 0;
    size_t sz  = L0T_SIZE;
    ptr_t va   = page_addr(ptep_pfn(ptep));

    ptep = mkl0tep(ptep);

    while (_n < n && va < VMAP_END) {
        pte_t pte = *ptep;
        if (pte_isnull(pte)) {
            _n += sz / base_sz;
        } 
        else if ((sz / LEVEL_SIZE) < base_sz) {
            _n = 0;
        }
        else {
            sz = sz / LEVEL_SIZE;
            ptep = ptep_step_into(ptep);
            continue;
        }

        if (ptep_vfn(ptep) + 1 == LEVEL_SIZE) {
            ptep = ptep_step_out(++ptep);
            va += sz;
            
            sz = sz * LEVEL_SIZE;
            continue;
        }
        
        va += sz;
        ptep++;
    }

    if (va >= VMAP_END) {
        return NULL;
    }

    va -= base_sz * _n;
    
    prev_va = va;
    return mkptep_va(ptep_vm_mnt(ptep), va);
}

ptr_t
vmap_ptes_at(pte_t pte, size_t lvl_size, int n)
{
    pte_t* ptep = mkptep_va(VMS_SELF, start);
    ptep = __alloc_contig_ptes(ptep, lvl_size, n);

    if (!ptep) {
        return 0;
    }

    vmm_set_ptes_contig(ptep, pte, lvl_size, n);

    ptr_t va = page_addr(ptep_pfn(ptep));

    tlb_flush_kernel_ranged(va, n);
    
    return va;
}

void
vunmap(ptr_t ptr, struct leaflet* leaflet)
{    
    pte_t* ptep;
    unsigned int npages;
 
    assert(start <= ptr && ptr <= VMAP_END);
    
    npages = leaflet_nfold(leaflet);
    ptep = mkptep_va(VMS_SELF, ptr);

    vmm_unset_ptes(ptep, npages);

    tlb_flush_kernel_ranged(ptr, npages);
}