#ifndef __LUNAIX_MEMPART64_H
#define __LUNAIX_MEMPART64_H

#define END_POINT(name) (name + name##_SIZE - 1)

#ifdef __LD__
#define __ulong(val) val
#else
#define __ulong(val) val##UL
#endif

#define __kaddr(x)                (__ulong(x) | 0xffff000000000000)

#define KSTACK_AREA             __ulong(0x0000000100000000)
#define KSTACK_AREA_SIZE        __ulong(0x0000000040000000)
#define KSTACK_AREA_END         END_POINT(KSTACK_AREA)

#define USR_EXEC                __ulong(0x0000008000000000)
#define USR_EXEC_SIZE           __ulong(0x0000002000000000)
#define USR_EXEC_END            END_POINT(USR_EXEC)

#define USR_MMAP                __ulong(0x0000018000000000)
#define USR_MMAP_SIZE           __ulong(0x00007e6000000000)
#define USR_MMAP_END            END_POINT(USR_MMAP)

#define USR_STACK               __ulong(0x00007fe000000000)
#define USR_STACK_SIZE          __ulong(0x2000000000)
#define USR_STACK_SIZE_THREAD   __ulong(0x0000000000200000)
#define USR_STACK_END           END_POINT(USR_STACK)


// la casa del kernel

#define KERNEL_RESIDENT         __kaddr(0xfb8000000000)     // -2.5T

#define PPAGE_POOL              __kaddr(0xfb8000000000)
#define PPAGE_POOL_SIZE         __ulong(0x40000000000)
#define PPAGE_POOL_END          END_POINT(PPAGE_POOL)

#define PMAP                    __kaddr(0xfff000000000)
#define PMAP_SIZE               __ulong(0xc00000000)        // 48G
#define PMAP_END                END_POINT(PMAP)

#define VMAP                    __kaddr(0xfffc00000000) 
#define VMAP_SIZE               __ulong(0x380000000)
#define VMAP_END                END_POINT(VMAP)

#define KERNEL_IMG              __kaddr(0xffff80000000)
#define KERNEL_IMG_SIZE         __ulong(0x80000000)
#define KERNEL_IMG_END          END_POINT(KERNEL_IMG)

#endif
