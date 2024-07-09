#ifndef __LUNAIX_MEMPART64_H
#define __LUNAIX_MEMPART64_H

#define END_POINT(name) (name + name##_SIZE - 1)

#ifdef __LD__
#define __ulong(val) val
#else
#define __ulong(val) val##UL
#endif

#define KSTACK_AREA             __ulong(0x0000000040000000)
#define KSTACK_AREA_SIZE        __ulong(0x0000000040000000)
#define KSTACK_AREA_END END_POINT(KSTACK_AREA)

#define USR_EXEC                __ulong(0x0000008000000000)
#define USR_EXEC_SIZE           __ulong(0x0000002000000000)
#define USR_EXEC_END END_POINT(USR_EXEC)

#define USR_MMAP                __ulong(0x0000010000000000)
#define USR_MMAP_SIZE           __ulong(0x0000008000000000)
#define USR_MMAP_END END_POINT(USR_MMAP)

#define USR_STACK               __ulong(0xfffffd6000000000)
#define USR_STACK_SIZE          __ulong(0x0000001fc0000000)
#define USR_STACK_END END_POINT(USR_STACK)

// 1GiB hole as page guard

#define KERNEL_RESIDENT         __ulong(0xfffffd8000000000)     // -2.5T
#define VMAP                    KERNEL_RESIDENT                 // -2.5T
#define VMAP_SIZE               __ulong(0x0000010000000000)
#define VMAP_END END_POINT(VMAP)

#define VMS_MOUNT_1             __ulong(0xfffffe8000000000)     // -1.5T
#define VMS_MOUNT_1_SIZE        __ulong(0x0000008000000000)
#define VMS_MOUNT_1_END END_POINT(VMS_MOUNT_1)

#define VMS_SELF_MOUNT          __ulong(0xffffff0000000000)     // -1T

#define KMAP                    __ulong(0xffffff8000000000)
#define PG_MOUNT_1              KMAP                            // -512G
#define PG_MOUNT_1_SIZE         __ulong(0x0000000000001000)
#define PG_MOUNT_1_END END_POINT(PG_MOUNT_1)

#define PG_MOUNT_2              __ulong(0xffffff8000001000)
#define PG_MOUNT_2_SIZE         __ulong(0x0000000000001000)
#define PG_MOUNT_2_END END_POINT(PG_MOUNT_2)

#define PG_MOUNT_3              __ulong(0xffffff8000002000)
#define PG_MOUNT_3_SIZE         __ulong(0x0000000000001000)
#define PG_MOUNT_3_END END_POINT(PG_MOUNT_3)

#define PG_MOUNT_4              __ulong(0xffffff8000003000)
#define PG_MOUNT_4_SIZE         __ulong(0x0000000000001000)
#define PG_MOUNT_4_END END_POINT(PG_MOUNT_4)

#define PG_MOUNT_VAR            __ulong(0xffffff8000004000)
#define PG_MOUNT_VAR_SIZE       __ulong(0x000000003fffc000)
#define PG_MOUNT_VAR_END END_POINT(PG_MOUNT_VAR)

#define PMAP                    __ulong(0xffffff8040000000)

#define KERNEL_IMG              __ulong(0xffffffff80000000)     // -2G
#define KERNEL_IMG_SIZE         __ulong(0x0000000080000000)
#define KERNEL_IMG_END END_POINT(KERNEL_IMG)


#endif