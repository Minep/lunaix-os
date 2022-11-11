#ifndef __LUNAIX_CONSTANTS_H
#define __LUNAIX_CONSTANTS_H

#define PG_SIZE_BITS 12
#define PG_SIZE (1 << PG_SIZE_BITS)
#define PG_INDEX_BITS 10

#define MEM_1MB 0x100000
#define MEM_4MB 0x400000

#define KSTACK_SIZE MEM_1MB
#define KSTACK_START ((0x3FFFFFU - KSTACK_SIZE) + 1)
#define KSTACK_TOP 0x3FFFF0U

#define KERNEL_MM_BASE 0xC0000000

#define KCODE_MAX_SIZE MEM_4MB
// #define KHEAP_START (KERNEL_MM_BASE + KCODE_MAX_SIZE)
// #define KHEAP_SIZE_MB 256

#define VGA_FRAMEBUFFER 0xB8000

#define KCODE_SEG 0x08
#define KDATA_SEG 0x10
#define UCODE_SEG 0x1B
#define UDATA_SEG 0x23
#define TSS_SEG 0x28

#define USER_START 0x400000
#define USTACK_SIZE 0x100000
#define USTACK_TOP 0x9ffffff0
#define USTACK_END (0x9fffffff - USTACK_SIZE + 1)
#define UMMAP_AREA 0x4D000000

#ifndef __ASM__
#include <stddef.h>
// From Linux kernel v2.6.0 <kernel.h:194>
/**
 * container_of - cast a member of a structure out to the containing structure
 *
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member)                                        \
    ({                                                                         \
        const typeof(((type*)0)->member)* __mptr = (ptr);                      \
        (ptr) ? (type*)((char*)__mptr - offsetof(type, member)) : 0;           \
    })

#endif
#endif /* __LUNAIX_CONSTANTS_H */
