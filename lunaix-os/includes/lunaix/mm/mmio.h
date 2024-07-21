#ifndef __LUNAIX_MMIO_H
#define __LUNAIX_MMIO_H

#include <lunaix/types.h>

ptr_t
ioremap(ptr_t paddr, u32_t size);

void
iounmap(ptr_t vaddr, u32_t size);

#endif /* __LUNAIX_MMIO_H */
