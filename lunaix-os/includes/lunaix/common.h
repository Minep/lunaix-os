#ifndef __LUNAIX_CONSTANTS_H
#define __LUNAIX_CONSTANTS_H

#define KSTACK_SIZE             (64 << 10)
#define KSTACK_START            ((0xFFBFFFFFU - KSTACK_SIZE) + 1)
#define KSTACK_TOP              0xffbffff0
#define HIGHER_HLF_BASE         0xC0000000
#define MEM_1MB                 0x100000

#define VGA_BUFFER_VADDR        0xB0000000
#define VGA_BUFFER_PADDR        0xB8000
#define VGA_BUFFER_SIZE         4096

#define KCODE_SEG              0x08
#define KDATA_SEG              0x10
#define UCODE_SEG              0x1B
#define UDATA_SEG              0x23
#define TSS_SEG                0x28

#define USER_START             0x400000
#define USTACK_SIZE            0x100000
#define USTACK_TOP             0x9fffffff
#define USTACK_END             (USTACK_TOP - USTACK_SIZE + 1)
#define UMMAP_AREA             0x4D000000

#define SYS_TIMER_FREQUENCY_HZ  2048

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
#define container_of(ptr, type, member) ({			\
        const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
        (type *)( (char *)__mptr - offsetof(type,member) );})

#endif
#endif /* __LUNAIX_CONSTANTS_H */
