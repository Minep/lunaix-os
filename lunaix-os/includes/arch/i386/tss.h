#ifndef __LUNAIX_TSS_H
#define __LUNAIX_TSS_H

#define tss_esp0_off 4

#ifndef __ASM__
#include <lunaix/types.h>
struct x86_tss
{
    u32_t link;
    u32_t esp0;
    u16_t ss0;
    u8_t __padding[94];
} __attribute__((packed));

void
tss_update_esp(u32_t esp0);
#endif

#endif /* __LUNAIX_TSS_H */
