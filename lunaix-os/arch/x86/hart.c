#include <lunaix/mm/vastm.h>
#include <lunaix/hart_state.h>
#include <klibc/string.h>

#include <asm/mempart.h>

bool
install_hart_transition(struct proc_mm* procvm, struct hart_transition* ht)
{
    pte_t *ptep;
    ptr_t mount_inject;
    
    ptep = vastm_walk_ptep_strict(
                vastm_procvm_root(procvm), ht->inject, RES_LFT);
    if (!ptep) 
        return false;

    mount_inject = leaflet_va(pte_leaflet(pte_at(ptep)));
    mount_inject += page_offset(ht->inject);
    memcpy((void*)mount_inject, &ht->transfer, sizeof(ht->transfer));
    
    return true;
}
