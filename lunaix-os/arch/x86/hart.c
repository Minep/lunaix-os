#include <lunaix/hart_state.h>
#include <lunaix/mm/vmm.h>
#include <klibc/string.h>

#include <asm/mempart.h>

bool
install_hart_transition(ptr_t vm_mnt, struct hart_transition* ht)
{
    pte_t pte;
    if (!vmm_lookupat(vm_mnt, ht->inject, &pte)) {
        return false;
    }

    mount_page(PG_MOUNT_4, pte_paddr(pte));

    ptr_t mount_inject = PG_MOUNT_4 + va_offset(ht->inject);
    memcpy((void*)mount_inject, &ht->transfer, sizeof(ht->transfer));
    
    unmount_page(PG_MOUNT_4);
    return true;
}