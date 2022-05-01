#ifndef __LUNAIX_CONSTANTS_H
#define __LUNAIX_CONSTANTS_H

#include <stddef.h>

#define K_STACK_SIZE            (64 << 10)
#define K_STACK_START           ((0xFFBFFFFFU - K_STACK_SIZE) + 1)
#define HIGHER_HLF_BASE         0xC0000000UL
#define MEM_1MB                 0x100000UL

#define VGA_BUFFER_VADDR        0xB0000000UL
#define VGA_BUFFER_PADDR        0xB8000UL
#define VGA_BUFFER_SIZE         4096

#define SYS_TIMER_FREQUENCY_HZ  2048


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

#endif /* __LUNAIX_CONSTANTS_H */
