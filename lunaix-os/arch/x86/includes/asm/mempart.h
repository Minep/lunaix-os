#ifndef __LUNAIX_MEMPART_H
#define __LUNAIX_MEMPART_H

#ifdef CONFIG_ARCH_X86_64
#   include "variants/mempart64.h"
#else
#   include "variants/mempart32.h"
#endif

#define PHYPAGE_MAP
#define PHYPAGE_MAP_START               PMAP
#define PHYPAGE_MAP_END                 PMAP_END
#define PHYPAGE_MAP_SIZE                PMAP_SIZE

#define VMEM_REMAP_ZONE 
#define VMEM_REMAP_ZONE_START           VMAP
#define VMEM_REMAP_ZONE_END             VMAP_END
#define VMEM_REMAP_ZONE_SIZE            VMAP_SIZE

#define PHYPAGE_POOL
#define PHYPAGE_POOL_START              PPAGE_POOL
#define PHYPAGE_POOL_END                PPAGE_POOL_END
#define PHYPAGE_POOL_SIZE               PPAGE_POOL_SIZE

#define KERNEL_STACK_ZONE
#define KERNEL_STACK_ZONE_START         KSTACK_AREA
#define KERNEL_STACK_ZONE_END           KSTACK_AREA_END
#define KERNEL_STACK_ZONE_SIZE          KSTACK_AREA_SIZE

#define USERLAND
#define USERLAND_START                  USR_EXEC
#define USERLAND_END                    USR_STACK_END
#define USERLAND_SIZE                   (USERLAND_END - USR_EXEC + 1)

#define USER_MMAP
#define USER_MMAP_START                 USR_MMAP
#define USER_MMAP_END                   USR_MMAP_END
#define USER_MMAP_SIZE                  USR_MMAP_SIZE

#define USER_STACK_ZONE
#define USER_STACK_ZONE_START           USR_STACK
#define USER_STACK_ZONE_END             USR_STACK_END
#define USER_STACK_ZONE_SIZE            USR_STACK_SIZE

#define mempart(part)                   part##_START
#define mempart_size(part)              part##_SIZE
#define mempart_end(part)               part##_END

#define KERNEL_AREA_BEGIN               KERNEL_RESIDENT
#define KERNEL_AREA_END                 -1UL

/*
 * TODO make these limit configurable
 */

// User stack maximum limit per thread
#define USER_STACK_UNITSIZE             USR_STACK_SIZE_THREAD 
#define KERNEL_STACK_UNITSIZE           ( 4 * 4096 )

#endif
