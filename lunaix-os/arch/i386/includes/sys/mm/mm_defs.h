#ifndef __LUNAIX_MM_DEFS_H
#define __LUNAIX_MM_DEFS_H

#include "mempart.h"

#define KSTACK_SIZE (2 * MEM_PAGE)

#define kernel_addr(addr) ((addr) >= KERNEL_EXEC)

#endif /* __LUNAIX_MM_DEFS_H */
