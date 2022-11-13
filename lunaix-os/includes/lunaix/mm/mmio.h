#ifndef __LUNAIX_MMIO_H
#define __LUNAIX_MMIO_H

#include <lunaix/types.h>

void*
ioremap(uintptr_t paddr, u32_t size);

void*
iounmap(uintptr_t vaddr, u32_t size);

#endif /* __LUNAIX_MMIO_H */
