#ifndef __LUNAIX_MEMPART32_H
#define __LUNAIX_MEMPART32_H

#define END_POINT(name) (name + name##_SIZE - 1)

#ifdef __LD__
#define __ulong(val) val
#else
#define __ulong(val) val##UL
#endif

#define KSTACK_AREA                 __ulong(0x100000)
#define KSTACK_AREA_SIZE            __ulong(0x300000)
#define KSTACK_AREA_END END_POINT(KSTACK_AREA)

#define USR_EXEC                    __ulong(0x400000)
#define USR_EXEC_SIZE               __ulong(0x20000000)
#define USR_EXEC_END END_POINT(USR_EXEC)

#define USR_MMAP                    __ulong(0x20400000)
#define USR_MMAP_SIZE               __ulong(0x9fbc0000)
#define USR_MMAP_END END_POINT(USR_MMAP)

#define USR_STACK                   __ulong(0xbffc0000)
#define USR_STACK_SIZE              __ulong(0x40000)
#define USR_STACK_SIZE_THREAD       __ulong(0x40000)
#define USR_STACK_END END_POINT(USR_STACK)

#define KERNEL_RESIDENT             __ulong(0xc0000000)

#define KERNEL_IMG                  KERNEL_RESIDENT
#define KERNEL_IMG_SIZE             __ulong(0x4000000)
#define KERNEL_IMG_END END_POINT(KERNEL_IMG)

#define KMAP                        __ulong(0xc8000000)
#define PG_MOUNT_1                  KMAP
#define PG_MOUNT_1_SIZE             __ulong(0x1000)
#define PG_MOUNT_1_END END_POINT(PG_MOUNT_1)

#define PG_MOUNT_2                  __ulong(0xc8001000)
#define PG_MOUNT_2_SIZE             __ulong(0x1000)
#define PG_MOUNT_2_END END_POINT(PG_MOUNT_2)

#define PG_MOUNT_3                  __ulong(0xc8002000)
#define PG_MOUNT_3_SIZE             __ulong(0x1000)
#define PG_MOUNT_3_END END_POINT(PG_MOUNT_3)

#define PG_MOUNT_4                  __ulong(0xc8003000)
#define PG_MOUNT_4_SIZE             __ulong(0x1000)
#define PG_MOUNT_4_END END_POINT(PG_MOUNT_4)

#define PG_MOUNT_VAR                __ulong(0xc8004000)
#define PG_MOUNT_VAR_SIZE           __ulong(0x3fc000)
#define PG_MOUNT_VAR_END END_POINT(PG_MOUNT_VAR)

#define VMAP                        __ulong(0xc8400000)
#define VMAP_SIZE                   __ulong(0x37400000)
#define VMAP_END END_POINT(VMAP)

#define PMAP                        VMAP

#define VMS_MOUNT_1                 __ulong(0xff800000)
#define VMS_MOUNT_1_SIZE            __ulong(0x400000)
#define VMS_MOUNT_1_END END_POINT(VMS_MOUNT_1)

#define VMS_SELF_MOUNT              __ulong(0xffc00000)

#endif /* __LUNAIX_MEMPART32_H */
