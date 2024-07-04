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

#define USR_STACK               __ulong(0x0000018000000000)
#define USR_STACK_SIZE          __ulong(0x0000000200000000)
#define USR_STACK_END END_POINT(USR_STACK)

#define KERNEL_IMG              __ulong(0xfffffe0000000000)
#define KERNEL_IMG_SIZE         __ulong(0x0000000400000000)
#define KERNEL_IMG_END END_POINT(KERNEL_IMG)

#define VMAP                    __ulong(0xfffffe0400000000)
#define VMAP_SIZE               __ulong(0x0000007c00000000)
#define VMAP_END END_POINT(VMAP)

#define PG_MOUNT_1              __ulong(0xfffffe8000000000)
#define PG_MOUNT_1_SIZE         __ulong(0x0000000000001000)
#define PG_MOUNT_1_END END_POINT(PG_MOUNT_1)

#define PG_MOUNT_2              __ulong(0xfffffe8000001000)
#define PG_MOUNT_2_SIZE         __ulong(0x0000000000001000)
#define PG_MOUNT_2_END END_POINT(PG_MOUNT_2)

#define PG_MOUNT_3              __ulong(0xfffffe8000002000)
#define PG_MOUNT_3_SIZE         __ulong(0x0000000000001000)
#define PG_MOUNT_3_END END_POINT(PG_MOUNT_3)

#define PG_MOUNT_4              __ulong(0xfffffe8000003000)
#define PG_MOUNT_4_SIZE         __ulong(0x0000000000001000)
#define PG_MOUNT_4_END END_POINT(PG_MOUNT_4)

#define PG_MOUNT_VAR            __ulong(0xfffffe8000004000)
#define PG_MOUNT_VAR_SIZE       __ulong(0x0000007fffffc000)
#define PG_MOUNT_VAR_END END_POINT(PG_MOUNT_VAR)

#define VMS_MOUNT_1             __ulong(0xffffff0000000000)
#define VMS_MOUNT_1_SIZE        __ulong(0x0000008000000000)
#define VMS_MOUNT_1_END END_POINT(VMS_MOUNT_1)

#endif