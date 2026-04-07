#ifndef __LUNAIX_MEMPART32_H
#define __LUNAIX_MEMPART32_H

#define END_POINT(name) (name + name##_SIZE - 1)

#ifdef __LD__
#define __ulong(val) val
#else
#define __ulong(val) val##UL
#endif

#define USR_EXEC                    __ulong(0x00400000)
#define USR_EXEC_SIZE               __ulong(0x40000000)
#define USR_EXEC_END END_POINT(USR_EXEC)

#define USR_MMAP                    __ulong(0x40400000)
#define USR_MMAP_SIZE               __ulong(0x97bffff0)
#define USR_MMAP_END END_POINT(USR_MMAP)

#define USR_STACK                   __ulong(0xd8000000)
#define USR_STACK_SIZE              __ulong(0x20000000)
#define USR_STACK_END               END_POINT(USR_STACK)


#define PPAGE_POOL                  __ulong(0x00000000)

#define KERNEL_RESIDENT             __ulong(0xf8000000)

#define KERNEL_IMG                  KERNEL_RESIDENT
#define KERNEL_IMG_SIZE             __ulong(0x02000000)
#define KERNEL_IMG_END END_POINT(KERNEL_IMG)

#define PMAP                        __ulong(0xfac00000)
#define PMAP_SIZE                   __ulong(0x04000000)
#define PMAP_END                    END_POINT(KMAP)

#define VMAP                        __ulong(0xfec00000)
#define VMAP_SIZE                   __ulong(0x01000000)
#define VMAP_END                    END_POINT(VMAP)

#define KSTACK_AREA                 __ulong(0xffc00000)
#define KSTACK_AREA_SIZE            __ulong(0x00400000)
#define KSTACK_AREA_END             END_POINT(KSTACK_AREA)

#endif /* __LUNAIX_MEMPART32_H */
