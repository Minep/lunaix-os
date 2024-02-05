#ifndef __LUNAIX_MM_DEFS_H
#define __LUNAIX_MM_DEFS_H

#include "mempart.h"

#define KSTACK_SIZE (3 * MEM_PAGE)

#define MEMGUARD 0xdeadc0deUL

#define kernel_addr(addr) ((addr) >= KERNEL_EXEC)
#define guardian_page(pte) ((pte) == MEMGUARD)

#endif /* __LUNAIX_MM_DEFS_H */
