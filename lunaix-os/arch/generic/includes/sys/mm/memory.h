#ifndef __LUNAIX_ARCH_MEMORY_H
#define __LUNAIX_ARCH_MEMORY_H

#include <lunaix/mm/pagetable.h>
#include <lunaix/mann_flags.h>

static inline pte_attr_t
translate_vmr_prot(unsigned int vmr_prot)
{
    return 0;
}


#endif /* __LUNAIX_ARCH_MEMORY_H */
