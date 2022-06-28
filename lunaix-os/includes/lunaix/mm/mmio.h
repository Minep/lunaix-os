#ifndef __LUNAIX_MMIO_H
#define __LUNAIX_MMIO_H

#include <stdint.h>

void*
ioremap(uintptr_t paddr, uint32_t size);

void*
iounmap(uintptr_t vaddr, uint32_t size);

#endif /* __LUNAIX_MMIO_H */
