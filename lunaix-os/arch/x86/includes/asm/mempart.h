#ifndef __LUNAIX_MEMPART_H
#define __LUNAIX_MEMPART_H

#ifdef CONFIG_ARCH_X86_64
#   include "variants/mempart64.h"
#else
#   include "variants/mempart32.h"
#endif

#define PHYPAGE_OCCUPANCY_MAP           PMAP
#define VADDR_RESOURCE_MAP              VMAP
#define PHYPAGE_DIRECT_MAP              PPAGE_POOL
#define KSTACK_RESIDENT_MAP             KSTACK_AREA

#define USERLAND_START                  USR_EXEC
#define USER_MMAP_START                 USR_MMAP
#define USER_STACK_ZONE_START           USR_STACK

#define mempart_size(part)              part##_SIZE
#define mempart_end(part)               part##_END
#endif
