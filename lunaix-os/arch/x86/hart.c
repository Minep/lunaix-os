#include <lunaix/mm/vastm.h>
#include <lunaix/hart_state.h>
#include <klibc/string.h>

#include <asm/mempart.h>

void
install_hart_transition(struct proc_mm* procvm, struct hart_transition* ht)
{
    pte_t *ptep;
    ptr_t mount_inject;
    
    ptep = vastm_walk_ptep_strict(
                vastm_procvm_root(procvm), ht->inject, RES_LFT);
    
    assert(ptep);

    mount_inject = phy_to_virt(pte_paddr(pte_at(ptep)));
    mount_inject += page_offset(ht->inject);
    memcpy((void*)mount_inject, &ht->transfer, sizeof(ht->transfer));
}
